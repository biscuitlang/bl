# Release Steps

- Create new branch `release/major.minor.patch`
- Run github `release` workflow on newly created branch.
- Download binaries from the workflow.
- Generate release documentation.
- Upload release documentation to `www-root/versions/major.minor.patch`
- Redirect web index to the new release index.
- Create github release.
  - Include link to the documentation.
  - Include change log.
  - Upload all build artifacts.
- Update master version to the next one.
  - Create new changelog entry.
  - Update project version in `CMakeLists.txt`.
  - Run tests.
  - Push master.
- Publish new release on github.
- Make sure everything works...
