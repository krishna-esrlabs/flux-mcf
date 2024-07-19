# Getting Started with MCF Python
* Requires python >= 3.8

## Installing Mcf Python
### Setup paths

* Add the path to mcf_tools to your PYTHONPATH (to make the change persist, add the command to your `.bashrc` file). 
The `mcf_python_path` package, which is in mcf_tools, contains script(s) which are imported by scripts within mcf_py 
which manage the MCF python paths automatically:

        export PYTHONPATH=$PYTHONPATH:/path/to/mcf_tools

### As pip package 

* To install Mcf Python as a python package:

        pip install /path/to/mcf_py

* To install as an editable package which will be automatically updated as source code is modified:

        pip install -e /path/to/mcf_py

### Not as pip package

* Install dependencies:
        
        pip install -r /path/to/mcf_py/requirements.txt


## Using Mcf Python
* All objects in the public API can be accessed using (there are the objects defined in `mcf_py/__init__.py`):

  ```python
  from mcf import PublicMcfClass
  ```

* If you want to access a private object:

  ```python
  from mcf.module_name import PrivateMcfClass
  ```

* If you didn't install MCF as a pip package, you can use either add mcf_py to your PYTHONPATH or use the
mcf_python_path package in your own script to manage the path for you:
  ```python
  import mcf_python_path.mcf_paths
  from mcf import RemoteControl
  
  rc = RemoteControl()
  ```