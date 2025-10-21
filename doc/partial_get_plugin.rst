.. _partial_get_plugin:

Partial Get Plugin Documentation for Access Layer 5 (AL5)
=========================================================

Overview
--------
The **Partial Get Plugin** is a C++ plugin for the Access Layer 5 (AL5) framework, designed to selectively retrieve data from an IMAS IDS (Interface Data Structure) based on user-defined ``includes`` and ``excludes`` queries. This document provides instructions for setup, compilation, and usage, along with detailed examples and syntax for query construction. The plugin operates on the ``core_profiles`` IDS in the provided example but can be adapted to other IDS structures.

For a comprehensive understanding of the AL5 plugin mechanism, refer to the official `AL5 documentation`.

Environment Setup
-----------------

To use this plugin, a High Level Interface is required, please refer to :

   - `C++ High Level Interface
     <https://sharepoint.iter.org/departments/POP/CM/IMDesign/Code%20Documentation/ACCESS-LAYER-doc/cpp/latest.html>`_
   - `Python High Level Interface
     <https://sharepoint.iter.org/departments/POP/CM/IMDesign/Code%20Documentation/ACCESS-LAYER-doc/python/latest.html>`_
   - `Fortran High Level Interface
     <https://sharepoint.iter.org/departments/POP/CM/IMDesign/Code%20Documentation/ACCESS-LAYER-doc/fortran/latest.html>`_
   - `Java High Level Interface
     <https://sharepoint.iter.org/departments/POP/CM/IMDesign/Code%20Documentation/ACCESS-LAYER-doc/java/latest.html>`_
   - `MATLAB High Level Interface
     <https://sharepoint.iter.org/departments/POP/CM/IMDesign/Code%20Documentation/ACCESS-LAYER-doc/matlab/latest.html>`_

Some important points regarding configuration and compilation for using plugins are recalled below.

To enable and use the Partial Get Plugin, set the following environment variables before running the examples:

.. code-block:: bash

   export IMAS_AL_ENABLE_PLUGINS=TRUE
   export IMAS_AL_PLUGINS=$HOME/AL_repos/al-cpp/build/_deps/al-plugins-build/

* **``IMAS_AL_ENABLE_PLUGINS``**: Must be set to ``TRUE`` to activate plugin support.
* **``IMAS_AL_PLUGINS``**: Must point to the directory containing the compiled plugin shared library (e.g., ``partial_get_plugin.so``), typically located in ``$HOME/AL_repos/al-cpp/build/_deps/al-plugins-build/`` after compilation.

Compilation Instructions
------------------------
To compile the C++ High-Level Interface (HLI) with plugin support, use for example the following ``cmake`` command:

.. code-block:: bash

   cmake -B build \
     -DAL_DOWNLOAD_DEPENDENCIES=OFF \
     -DAL_PLUGINS=ON \
     -DCMAKE_INSTALL_PREFIX=~/al-install/ \
     -DAL_DEVELOPMENT_LAYOUT=ON \
     -DCMAKE_BUILD_TYPE=Debug

* **Key Flag**: ``-DAL_PLUGINS=ON`` enables plugin support. Ensure this is set when building the project.


How the Partial Get plugin works
--------------------------------

The **Partial Get Plugin** filters the Read requests sent by High Level Interfaces (HLI) to the backend through the Low level (LL). 
When an HLI sends a request to access an array of structures (AOS) or a IMAS Data Dictionary data field, the plugin intercepts the request 
to check if the request field path belongs to a specific set of paths (designed as ``E`` below) which is defined by the users 
through "includes" and "excludes" queries. Both queries are two string parameters of the plugin.
The ``E`` set represents the set of all valid paths as defined by the "includes" queries minus the paths explicitly removed by 
the "excludes" queries. A path is considered to be in ``E`` if it matches at least one include query and does not match any 
exclude query. This concept is used by the plugin to determine whether a specific path or AOS is included in the managed set of paths.


The Partial Get parameters "includes" and "excludes" 
----------------------------------------------------

Syntax and Examples
-------------------

These parameters are semicolon-separated (``;``) lists of paths that can target arrays of structures (AOS) or individual structures or individual fields.

Parameters ``includes`` and ``excludes`` follow a specific syntax based on the IMAS Data Dictionary (DD) paths.

Syntax Rules
~~~~~~~~~~~~
1. **Array of Structures (AOS) Queries**:
   - **Format**: ``aos_name(mi:ma:k)``
     - ``mi``: Minimum index (≥ 1).
     - ``ma``: Maximum index (≥ 1).
     - ``k``: Increment (≥ 0).
     - Selects indices ``mi, mi+k, mi+2*k, ..., mi+n*k`` where ``mi+n*k ≤ ma``.

   - **Example 1**: ``profiles_1d(1:3:1)`` → Selects indices ``{1, 2, 3}``.
   - **Example 2**: ``profiles_1d(:4)`` → Selects indices ``{1, 2, 3, 4}``.
   - **Example 3**: ``profiles_1d(3:)`` → All aos indices starting from 3.
   - **Example 4**: ``profiles_1d(::2)`` → All aos indices from 1 by step of 2 ``{1, 3, 5, ...}``.
   - **All Elements**: ``aos_name(:)`` (e.g., ``profiles_1d(:)``).
   - **Nested AOS**: Use ``/`` to separate levels (e.g., ``profiles_1d(1:5:2)/ion(1:4:2)``).
   - **Effect**:
     - In ``includes``: Returns all data under ``aos_name(i)/`` for each selected index ``i``.
     - In ``excludes``: Excludes all data under ``aos_name(i)/`` for each selected index ``i``.

2. **Structure Queries**:
   - **Format**: ``structure_name`` or ``parent_structure/structure_name``.
   - **Example**: ``ids_properties`` or ``profiles_1d(:)/grid``.
   - **Effect**:
     - In ``includes``: Returns all data under ``structure_name/``.
     - In ``excludes``: Excludes all data under ``structure_name/``.

3. **Field Queries**:
   - **Format**: ``field_name`` or ``parent_structure/field_name``.
   - **Example**: ``time`` or ``profiles_1d(:)/time``.
   - **Effect**:
     - In ``includes``: Returns only the field data from ``field_name``.
     - In ``excludes``: Excludes only the field data from ``field_name``.

4. **Notes**:
   - Indices follow Fortran convention (start at 1).
   - The plugin does not validate paths against the DD but requires correct AOS syntax (e.g., indices must be specified).
   - ``excludes`` must be a subset of ``includes``.
   - If ``includes`` is empty, all data are returned by default (except data excluded by ``excludes`` queries).

Examples for ``core_profiles`` IDS
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
.. list-table::
   :widths: 40 60
   :header-rows: 1

   * - Query
     - Description
   * - ``"ids_properties"``
     - Returns all data in the ``ids_properties`` structure.
   * - ``"profiles_1d(1:3:1)"``
     - Returns data for ``profiles_1d`` elements at indices 1, 2, and 3 (i.e., ``profiles_1d(1)/``, etc.).
   * - ``"profiles_1d(1:3)"``
     - Same effect as ``"profiles_1d(1:3:1)"``.
   * - ``"profiles_1d(3:)"``
     - Returns data for ``profiles_1d`` elements for indices >=3.
   * - ``"profiles_1d(::10)"``
     - Returns data for ``profiles_1d`` elements for indices >=1 with step 10 (i.e., ``profiles_1d(1)/``, ``profiles_1d(11)/``, ``profiles_1d(21)/``, etc.).
   * - ``"ids_properties;profiles_1d(1:3:1)"``
     - Combines the above: includes ``ids_properties`` and ``profiles_1d`` at indices 1, 2, and 3.
   * - ``"ids_properties;profiles_1d(:)/ion(1:4:2)"``
     - Includes ``ids_properties`` and ``ion`` elements at indices 1 and 3 for all ``profiles_1d`` elements.
   * - ``"ids_properties;profiles_1d(1:5:2)/ion(1:4:2)"``
     - Includes ``ids_properties`` and ``ion`` at indices 1 and 3 for ``profiles_1d`` at indices 1, 3, 5.
   * - ``"ids_properties;profiles_1d(1:5:2)/ion(1:4:2);profiles_1d"``
     - Adds all ``profiles_1d`` data (0D-6D datasets and structures) beyond the specific ``ion`` elements.
   * - ``"ids_properties;profiles_1d(:)/grid"``
     - Includes ``ids_properties`` and the ``grid`` structure for all ``profiles_1d`` elements.
   * - ``"ids_properties;profiles_1d(1:5:2)/ion(1:4:2);profiles_1d(1:5:2)/grid"``
     - Combines ``ion`` at indices 1 and 3, ``grid`` for ``profiles_1d`` at indices 1, 3, 5, and ``ids_properties``.

.. note::

   The shapes of (included) arrays of structures (AOS) are never modified. However, the AOS elements not included by the user queries are not set.

Technical Notes
---------------
1. **Query Concatenation**: Multiple paths in ``includes`` or ``excludes`` are separated by ``;``. Each path must align with DD conventions since the plugin does not perform validation against the Data Dictionary.
2. **AOS Requirement**: For AOS queries, indices must always be specified (e.g., ``profiles_1d(:)`` for all elements). Invalid queries like ``profiles_1d/ion(1:4:2)`` (missing ``profiles_1d`` indices) are not supported.
3. **Excludes Subset**: Data in ``excludes`` must be a subset of ``includes`` to be effective.
4. **PathManager component**: Internally, the plugin uses ``PathManager`` to evaluate paths:

   - ``isInSet``: Checks if a specific path is in the included set ``E``.
   - ``is_included``: Verifies if an AOS has at least one element in ``E``.

The Partial Get parameter "debug"
---------------------------------
The "debug" parameter  allows to display how HLI read requests are filtered by the plugin. For each request, the plugin displays if the read request is 
rejected or not, according to the user defined "includes" and "excludes" queries.

Troubleshooting
---------------
- **Plugin Not Loading**: Verify ``IMAS_AL_PLUGINS`` points to the correct directory and that ``partial_get_plugin.so`` exists.
- **No Data Returned**: Check query syntax. Enable ``debug`` mode to check if the field is excluded by the plugin.