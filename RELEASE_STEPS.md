# Release Steps

- Checkout master.
- Update CHANGELOG with master->version number.
- Remove obsolete stuff. All functions marked as `#obsolete "Since release-version..."`.
  We remove obsolete stuff which is marked with previous release version number.
- Create new branch `release/major.minor.patch`
- Checkout release branch.
- In docs/src/index.md replace installation section:

	# Installation

	* Download compiler from [Github](https://github.com/biscuitlang/bl/releases/tag/VERSION).
	* Unpack downloaded file.
	* Optionally add `/path/to/blc/bin` to your system `PATH`.
	* Run `blc --help`.

- Run ./build.bat docs to generate new documentation.
- Copy new documentation to webside (from: docs/side to: www/versions/version-number).
- Push release branch.
- Wait for release CI job to finish.
- Check CI test results and verify created artifacts.
- Create github release (format based on previous one).
- Include artifacts to the release.
- Publish release.

# Post release steps

- Checkout master.
- Update links in `docs/index.html`.
- Publish new index to web.
- Verify it works.
- Increase compiler verison to the next one.
- Run `./build.bat all` and verify the version.
- Add new changelog section:

	master
	---------------------------------------------------------------------------------------
	[Compiler]
	[Modules]
	[Documentation]
	[Deprecated]

- Push master branch.
- Make sure everything works...