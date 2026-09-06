# Useful sites: https://isocpp.org/wiki/faq

# Standards
1. Avoid using raw pointer and 'new' to allocate memory unless there is a good reason. 
   In other words, shared_ptr or unique_ptr should be always used to manage ownership. 
   If there is no notion of ownership, raw pointer should be used.

2. Naming standards:
 - File name: FileName.cpp, HeaderName.h
 - Class/structure name: ClassName
 - Variable name: variableName
 - Function/method name: this_is_a_function_name

3. 'using namespace amrex' is allowed in *.cpp files. Otherwise, do NOT leave 
   'using namespace xxx' in the code. 

4. For beauty, the order of headers: std headers -> AMReX headers -> user headers. 
    The behavior of the code shoud NOT depend on the order of headers.      

5. Use 'nullptr' instead of NULL for pointer. 

6. Always use 'const' if possible. 

7. Lambda is useful, but a regular function is better if it is universal or it is long. 

8. Using debug flags and Valgrind to check errors.

9. Follow the coventional commits format: https://www.conventionalcommits.org/en/v1.0.0/

