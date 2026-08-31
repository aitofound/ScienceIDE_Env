"""Thin active validator."""
import importlib.util,os
p=os.path.abspath(os.path.join(os.path.dirname(__file__),'..','..','validate-common.py'));s=importlib.util.spec_from_file_location('common_srblast',p);m=importlib.util.module_from_spec(s);s.loader.exec_module(m)
def validate(reference_paths,candidate_paths):return m.validate_check('srblast','srblast','minkowski',reference_paths,candidate_paths)
