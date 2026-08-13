# Repository Bootstrap

## Create the GitHub repository

Suggested repository name: `Track-Session-Timer-GPS-`

After extracting this archive:

```bash
cd Track-Session-Timer-GPS-
git init
git add .
git commit -m "Initial TrackSessionTimer GPS project architecture"
git branch -M main
git remote add origin git@github.com:<owner>/Track-Session-Timer-GPS-.git
git push -u origin main
```

If you create the GitHub repository with a README or licence already present, reconcile that first rather than force-pushing over it.

## Then clone as normal

```bash
git clone git@github.com:<owner>/Track-Session-Timer-GPS-.git
cd Track-Session-Timer-GPS-
```

## Seed labels and issues

Install/authenticate GitHub CLI. Create the standard labels first:

```bash
python tools/create_labels.py
python tools/create_labels.py --execute --repo <owner>/<repo>
```

Then preview the issues:

```bash
python tools/create_issues.py
```

When satisfied:

```bash
python tools/create_issues.py --execute --repo <owner>/<repo>
```

Issue execution skips titles already present. The snapshot scripts do not recreate
native sub-issue or blocked-by relationships; use the GitHub API if cloning the full
planning hierarchy into another repository.

## Recommended first branch

```bash
git switch -c feature/2-repository-hygiene
```

Complete the M0 repository/toolchain gate before beginning board-specific display work.
