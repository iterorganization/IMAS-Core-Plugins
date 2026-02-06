# IMAS-Core Plugins

This repository contains some plugins examples that can be used with the IMAS-Core library (and the associated high-level interfaces).

> [!WARNING]
> Plugins are still an experimental feature, use this feature with caution

To enable the plugins framework, set the cmake configuration variable `AL_PLUGINS=ON`.

These plugins can be bound manually to the [IMAS-Core](https://github.com/iterorganization/IMAS-Core) library to extend its features. 
The procedure to execute these plugins is documented below. 

## Build info

In order to compile and try these plugins examples, you need to set the global variables `AL_BUILD` and `IMAS_AL_PLUGINS`.

### Compiling plugins into shared libraries

As usual, `AL_BUILD` is the location of the Access Layer main directory which contains the `IMAS-Cpp` directory. 
To compile all the examples:
```
make -f Makefile_examples
```
You can also compile only one example, for instance to compile the 'creation_date_plugin' example:
```
make -f Makefile_examples test_creation_date_plugin
```

### Installing plugins libraries

Create a directory which will contain the plugins shared libraries, for example:
```
mkdir $HOME/access_layer_plugins
```
Move all the shared library .so files built in step 1 in this directory.

In order to use the plugins shared libraries (.so files), you need to set the `IMAS_AL_PLUGINS` environment variable. 
This variable is used by the IMAS-Core library to find the available plugins shared libraries.
`IMAS_AL_PLUGINS` is the directory created above that contains the plugins .so shared libraries, e.g:
```
export IMAS_AL_PLUGINS=$HOME/access_layer_plugins
```

### Testing the plugins 

First, compile all plugins and Fortran/C++ examples:
```
make -f Makefile_examples
make test
```

#### `debug` plugin

This plugin can be executed on an existing pulse file.
Using C++, the following command:
```
./test_debug_plugin imas:hdf5?path=$WEST_PULSE_FILES/54914/0/
```
will execute the 'debug_plugin' for pulse 54914 whose the 'master.h5' file is located in directory `$WEST_PULSE_FILES/54914/0`.
This will call the 'debug' plugin (using the get() operation)  for pulse 54914, run 0, at the specified path.
All examples below use HDF5, so the specified path should be a directory containing the 'master.h5' file. 

Or using Fortran:
```
./test_fortran_debug_plugin imas:hdf5?path=$WEST_PULSE_FILES/54914/0/
```

Or using Python:
```
python test_debug_plugin.py imas:hdf5?path=$WEST_PULSE_FILES/54914/0/
```

#### `partial_get` plugin

This plugin can be executed on an existing pulse file.
Using C++, the following command:
```
./test_partial_get_plugin imas:hdf5?path=$WEST_PULSE_FILES/54914/0/
```
will execute the 'test_partial_get_plugin' for pulse 55000 whose the 'master.h5' file is located in directory `$WEST_PULSE_FILES/54914/0`.
This will call the 'partial_get' plugin, reading the pulse file for pulse 54914 at the specified path (folder containing a 'master.h5' file).

Or using Fortran:
```
./test_fortran_partial_get_plugin imas:hdf5?path=$WEST_PULSE_FILES/54914/0/
```

Or using Python:
```
python test_partial_get_plugin.py imas:hdf5?path=$WEST_PULSE_FILES/54914/0/
```
 
#### `creation_date` plugin

This plugin will create a new pulse file.
Using C++, execute the following command:
```
./test_creation_date_plugin imas:hdf5?path=$MY_PULSE_FILES/55000/0
```
This will call the `creation_date` plugin, writing the pulse file for pulse 55000 at the specified path.
The field `ids_properties/creation_date` will be patched for the magnetics IDS, occurrence 0. 

Or using Fortran:
```
./test_fortran_creation_date_plugin imas:hdf5?path=$MY_PULSE_FILES/55000/0
```

Or using Python:
```
python test_creation_date_plugin.py imas:hdf5?path=$MY_PULSE_FILES/55000/0
```


## Contributors

These plugins have been developed by [Ludovic Fleury](https://github.com/fleuryl-ai) (Copyright 2023-2025, CEA, France).
