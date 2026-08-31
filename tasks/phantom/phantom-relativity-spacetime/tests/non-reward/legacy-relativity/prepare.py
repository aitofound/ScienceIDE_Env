#!/usr/bin/env python3
"""Patch only declared generated input options, never source."""
import json,re,sys
def patch(path,vals):
 text=open(path,encoding='utf-8').read()
 for key,val in vals.items():
  pat=re.compile(r'^(\s*'+re.escape(key)+r'\s*=\s*)\S+(.*)$',re.M)
  text,n=pat.subn(lambda m:m.group(1)+str(val)+m.group(2),text)
  if n!=1:raise RuntimeError('%s occurs %d times in %s'%(key,n,path))
 with open(path,'w',encoding='utf-8',newline='\n') as f:f.write(text)
case,section,path=sys.argv[1:];c=json.load(open(case,encoding='utf-8'))
patch(path,c.get('setup_overrides',{}) if section=='setup' else {'tmax':c['tmax'],'dtmax':c['dtmax'],'nfulldump':'1'})
