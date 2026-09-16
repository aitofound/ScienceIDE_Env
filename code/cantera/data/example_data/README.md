# Cantera Example Data Files

This repository provides data files used by various Cantera examples, which can be seen
at https://cantera.org/stable/examples/python/index.html. In particular, many of the
input files found here are based on scientific publications and are meant to demonstrate
Cantera's capabilities with more realistic and interesting reaction systems than those
based only on the basic mechanisms provided in the main Cantera repository.

> [!IMPORTANT]
> **Licensing and Attribution Notice**
>
> The Cantera project is not the original author of the reaction mechanisms included
> in this repository and is not claiming to grant a license to them. The mechanisms
> were assembled by their respective researchers and appear to be shared without
> restrictive licensing requirements.
>
> **If you use this data in scientific publications, please cite the original papers**
> associated with each mechanism. Suitable citations are usually included in the
> `description` field of each mechanism file.

## Adding files for new examples

To implement an example requiring a new input file, make sure to have forked both
`Cantera/cantera` and `Cantera/cantera-example-data` repositories. Steps outlined below
must use the local Git checkouts of your personal forks.

### Create example and input file(s)

- Create a new feature branch in your personal fork of the `cantera` repository.
- Create your new example in your local Git checkout of Cantera.
- In the `data/example_data` subdirectory (a Git submodule), configure your fork of
  `cantera-example-data` as a remote and create a feature branch for your input file(s).
- Create the new input file(s) in the `data/example_data` subdirectory.
- Verify that your example performs as expected locally.

### Commit new files to your forks

- From within the `data/example_data` subdirectory, commit the input file(s) to your
  feature branch in your `cantera-example-data` repository.
- From the root folder, commit the example to your feature branch in your `cantera`
  repository.
  * :warning: Make sure you _do not_ add the input file(s) or an update of the
    `example_data` submodule to this branch.

### Create pull requests on GitHub

- Create a pull request for your example in the main `Cantera/cantera` repository.
  * You should see multiple failing jobs. For new Python examples, descriptions start
    with `CI / Run Python 3.XX examples on ubuntu-YY.ZZ` for tests with different
    Python and ubuntu versions. For examples using other Cantera APIs, multiple other CI
    jobs may fail.
- Create a pull request in the `Cantera/cantera-example-data` repository with the
  reference `(Cantera/cantera#XYZ)` in the title, where `XYZ` is the
  number of your PR in the main repository.
- Subsequent pushes to your feature branch on the main PR will result in the following:
  * The CI process in the main repository will automatically check out this submodule
    PR so the new input file is available for those jobs that run the examples.
  * You should see one failing job, with the description `Linters / Check for unmerged
    example-data pull request (pull_request)`. This is a reminder for maintainers for
    the remaining steps that need to be completed before merging the PR.
- Once the PR review of the `cantera-example-data` branch is completed, it will be
  merged by maintainers.
- Finally, the PR branch for the main repository can be updated to include the update
  of the `example_data` submodule, which should be reset to the official Cantera
  `cantera-example-data` repository. At this point, all CI jobs should pass. Once the
  PR review is completed, the PR is ready to be merged.
