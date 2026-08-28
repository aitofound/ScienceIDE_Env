#!/usr/bin/env python

from setuptools import setup, find_packages

setup(name='pyPLUTO',
      version='4.4.2',
      description="Python Visualisation module for PLUTO v4.4. Upgraded to Python 3.x including particle files reader",
      author="Bhargav Vaidya",
      author_email="bvaidya@iiti.ac.in",
      url="www.iiti.ac.in/people/~bvaidya/codes.html",
      packages=find_packages(),
      classifiers=[
        "Programming Language :: Python :: 3",
        "License :: OSI Approved :: MIT License",
        "Operating System :: OS Independent",
    	],
      python_requires='>=3.6',
     )

