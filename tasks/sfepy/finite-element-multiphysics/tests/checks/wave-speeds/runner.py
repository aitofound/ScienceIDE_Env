"""Execute a trusted upstream case and export only explicitly selected physics.

All instrumentation is inserted into the trusted case, never into production
functions. Production assertions and calls create no observations by themselves.
"""
import ast, inspect, json, os, sys, types
from pathlib import Path
import numpy as np

HERE=Path(__file__).resolve().parent
SPEC=json.loads((HERE/'case.json').read_text())
INPUT=json.loads(Path(sys.argv[1]).read_text())
SCALE=float(INPUT['scale'])
RECORDS={}
COUNTS={}

def put(name, values, coordinates=None):
    arr=np.asarray(values,dtype=np.float64)
    if not arr.size or not np.isfinite(arr).all():
        raise ValueError('empty/nonfinite physical observation '+name)
    idx=COUNTS.get(name,0);COUNTS[name]=idx+1
    item={'values':arr.tolist(),'shape':list(arr.shape)}
    if coordinates is not None:
        keys=np.asarray(coordinates,dtype=np.float64)
        if len(keys)!=len(arr): raise ValueError('coordinate/value length mismatch '+name)
        item['coordinates']=keys.tolist()
    RECORDS[f'{name}/{idx}']=item

def field(name,var):
    vals=np.asarray(var()).reshape((-1,var.n_components))
    put(name,vals,var.field.get_coor())

def state(name,variables):
    for var in variables:
        if var.is_state() and var.field is not None: field(name+'/'+var.name,var)

def capture(name,value,loc):
    kind=SPEC['captures'].get(name)
    if kind is None:return
    if kind=='state': state(name,value)
    elif kind=='field':field(name,value)
    elif kind=='mass':put(name,float(value.sum()))
    elif kind=='quadrature':
        from sfepy.discrete.common.mappings import get_physical_qps
        region=loc['yrig'] if 'yrig' in loc else loc['data'].omega
        coors=get_physical_qps(region,loc['integral']).values
        put(name,np.asarray(value).reshape((len(coors),-1)),coors)
    elif kind=='vector':
        variables=loc.get('variables') or loc.get('data').variables
        for var in variables:
            if var.is_state():
                sl=variables.di.indx[var.name]
                put(name+'/'+var.name,np.asarray(value[sl]).reshape((-1,var.n_components)),var.field.get_coor())
    elif kind.startswith('at:'):
        put(name,value,loc[kind[3:]])
    elif kind=='projected-u':put(name,np.asarray(value).reshape((-1,loc['u'].n_components)),loc['u'].field.get_coor())
    else:put(name,value)

class Instrument(ast.NodeTransformer):
    def __init__(self):self.function=''
    def visit_FunctionDef(self,node):
        previous=self.function;self.function=node.name
        node=self.generic_visit(node);self.function=previous
        return node
    def visit_Assert(self,node):
        # Literal expected answers of an upstream test are for its nominal inputs.
        # Variant runs retain the same production calls but calibrate perturbed inputs.
        return ast.copy_location(ast.Pass(),node) if SCALE!=1 else node
    def visit_Assign(self,node):
        node=self.generic_visit(node)
        names=[]
        for t in node.targets:
            names += [n.id for n in ast.walk(t) if isinstance(n,ast.Name)]
        if SCALE!=1 and any(n in SPEC.get('scaled_assignments',[]) for n in names):
            node.value=ast.Call(func=ast.Name(id='_scale_input',ctx=ast.Load()),args=[node.value],keywords=[])
        extras=[]
        for name in names:
            if name in SPEC['captures'] and self.function in SPEC['functions'] and SPEC['captures'][name] not in ('field','vector'):
                extras.append(ast.Expr(ast.Call(func=ast.Name(id='_capture',ctx=ast.Load()),args=[ast.Constant(name),ast.Name(id=name,ctx=ast.Load()),ast.Call(func=ast.Name(id='locals',ctx=ast.Load()),args=[],keywords=[])],keywords=[])))
        return [node,*extras]
    def visit_Call(self,node):
        node=self.generic_visit(node)
        if SCALE == 1:return node
        f=node.func
        if isinstance(f,ast.Name) and f.id == 'ElasticConstants':
            for kw in node.keywords:
                if kw.arg in ('lam','mu'):kw.value=ast.Call(func=ast.Name(id='_scale_input',ctx=ast.Load()),args=[kw.value],keywords=[])
        if isinstance(f,ast.Name) and f.id in ('EssentialBC','InitialCondition') and len(node.args)>2 and isinstance(node.args[2],ast.Dict):
            node.args[2].values=[ast.Call(func=ast.Name(id='_scale_input',ctx=ast.Load()),args=[v],keywords=[]) for v in node.args[2].values]
        if isinstance(f,ast.Attribute) and f.attr=='set_constant' and node.args:
            node.args[0]=ast.Call(func=ast.Name(id='_scale_input',ctx=ast.Load()),args=[node.args[0]],keywords=[])
        if isinstance(f,ast.Attribute) and f.attr=='set_from_function' and node.args:
            node.args[0]=ast.Call(func=ast.Name(id='_scaled_function',ctx=ast.Load()),args=[node.args[0]],keywords=[])
        return node
    def visit_Expr(self,node):
        node=self.generic_visit(node)
        if isinstance(node.value,ast.Call) and isinstance(node.value.func,ast.Name) and node.value.func.id in ('project_by_component','make_l2_projection'):
            target=node.value.args[0]
            if isinstance(target,ast.Name) and target.id in SPEC['captures']:
                return [node,ast.parse(f"_capture('{target.id}', {target.id}, locals())").body[0]]
        if not isinstance(node.value,ast.Call) or not isinstance(node.value.func,ast.Attribute):return node
        fun=node.value.func
        extra=None
        if self.function in SPEC['functions'] and 'vec' in SPEC['captures'] and fun.attr in ['apply_ebc','apply_ic']:
            extra=ast.parse("_capture('vec', vec, locals())").body[0]
        if self.function in SPEC['functions'] and fun.attr=='solve' and isinstance(fun.value,ast.Name) and fun.value.id=='pb':
            extra=ast.parse("_state('solution', pb.get_variables())").body[0]
        return [node,extra] if extra is not None else node

def scale_input(v):
    if isinstance(v,(float,int,np.ndarray,np.number)):return v*SCALE
    return v

# Force the named deterministic direct backend for every upstream constructor.
from sfepy.solvers.ls import ScipyDirect
original_init=ScipyDirect.__init__
def direct_init(self,conf,**kwargs):
    if isinstance(conf,dict):conf=dict(conf,method='superlu')
    else:
        conf=conf.copy();conf.method='superlu'
    return original_init(self,conf,**kwargs)
ScipyDirect.__init__=direct_init

if SPEC.get('mesh_scale') and SCALE!=1:
    from sfepy.discrete.fem import Mesh
    original_mesh=Mesh.from_file
    def scaled_mesh(*args,**kwargs):
        mesh=original_mesh(*args,**kwargs)
        mesh.coors[:]*=SCALE
        return mesh
    Mesh.from_file=staticmethod(scaled_mesh)
case_path=HERE/'upstream.py'
module=types.ModuleType('trusted_case');module.__file__=str(case_path)
sys.modules[module.__name__]=module
module.__dict__.update(_capture=capture,_state=state,_scale_input=scale_input,_scaled_function=lambda f: lambda *a,**kw:f(*a,**kw)*SCALE)
tree=ast.fix_missing_locations(Instrument().visit(ast.parse(case_path.read_text())))
exec(compile(tree,str(case_path),'exec'),module.__dict__)

if SPEC.get('scale_config_bc') and SCALE != 1:
    for name,value in module.__dict__.copy().items():
        if name.startswith('ebc_') and isinstance(value,dict):
            value['dofs']={k:scale_input(v) for k,v in value['dofs'].items()}

# Explicit coordinate-defined field data replaces the upstream random initial vector.
if SPEC.get('fixed_initial_vector'):
    def init_vec(variables):
        out=np.empty(variables.di.n_dof_total)
        for var in variables:
            coors=var.field.get_coor()
            vals=(0.25+np.sum(coors*coors,axis=1))[:,None]+0.125*np.arange(var.n_components)[None,:]
            out[variables.di.indx[var.name]]=(vals*SCALE).ravel()
        return out
    module.init_vec=init_vec
if SPEC.get('elastic_sensitivity_only'):
    module.test_terms=[t for t in module.test_terms if t[1]=='dw_lin_elastic']

kwargs={}
func=getattr(module,SPEC['selector'])
for param in inspect.signature(func).parameters:
    if param=='output_dir':kwargs[param]=str(Path.cwd())
    elif param=='mesh_filename':kwargs[param]=SPEC['mesh_filename']
    else:
        fixture=getattr(module,param)
        actual=getattr(fixture,'__wrapped__',fixture)
        fkwargs={k:str(Path.cwd()) for k in inspect.signature(actual).parameters if k=='output_dir'}
        kwargs[param]=actual(**fkwargs)
func(**kwargs)
if not RECORDS:raise RuntimeError('trusted case exported no physical quantities')
Path(os.environ['OUT_DIR']).mkdir(parents=True,exist_ok=True)
Path(os.environ['OUT_DIR'],'physics.json').write_text(json.dumps(RECORDS,allow_nan=False,sort_keys=True)+'\n')
