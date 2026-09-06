"""
Print the name of the wheel file, needed when packaging with CPack
See: https://stackoverflow.com/a/60644659
"""

from setuptools import Extension
from setuptools.dist import Distribution


sdf = Extension('sdf', ['fuzz.pyx'])  # the files don't need to exist
dist = Distribution(attrs={'name': 'sdfpy', 'version': '1.0', 'ext_modules': [sdf]})
bdist_wheel_cmd = dist.get_command_obj('bdist_wheel')
bdist_wheel_cmd.ensure_finalized()

distname = bdist_wheel_cmd.wheel_dist_name
tag = '-'.join(bdist_wheel_cmd.get_tag())
wheel_name = f'{distname}-{tag}.whl'
print(wheel_name)
