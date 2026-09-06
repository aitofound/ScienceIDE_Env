---
description: How to add a new PARAM.XML command end-to-end
---

# Add a New Parameter Command

This workflow walks through adding a new `#COMMANDNAME` to FLEKS, from
PARAM.XML documentation to code implementation.

## Steps

1. **Document the command in `PARAM.XML`:**
   Add a `<command>` block. Follow the existing format:
   ```xml
   <command name="COMMANDNAME"
            alias="COMMANDNAME_FLEKS0,COMMANDNAME_FLEKS1,COMMANDNAME_FLEKS2"
            multiple="T">
     <parameter name="paramName" type="real" default="1.0"/>

   #COMMANDNAME
   1.0            paramName

   Description of what this command does.
   </command>
   ```
   - `multiple="T"` allows per-domain overrides (FLEKS0, FLEKS1, etc.)
   - Alias names must follow the pattern `COMMANDNAME_FLEKS{0,1,2}`

2. **Add the member variable** to the appropriate header file:
   - `include/Pic.h` for PIC-related parameters
   - `include/FluidInterface.h` for coupling/fluid parameters
   - `include/Domain.h` for domain-level parameters
   - `include/TimeCtr.h` for time-related parameters

3. **Parse the command in `read_param()`:**
   - Open the corresponding `.cpp` file (e.g., `src/Domain.cpp`, `src/Pic.cpp`)
   - Find the `read_param()` method
   - Add a new `else if` block matching your command name:
   ```cpp
   } else if (command == "#COMMANDNAME") {
     read_var("paramName", paramName);
   }
   ```

4. **Use the parameter** in the relevant simulation logic.

5. **Test:** Run `make test16_3d` from SWMF root to verify nothing breaks.

6. **Commit** using conventional format:
   ```
   feat: add #COMMANDNAME for <description>
   ```
