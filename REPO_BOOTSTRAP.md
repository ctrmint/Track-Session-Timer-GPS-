# Repository Bootstrap

## Create the GitHub repository

Suggested repository name: `TrackSessionTimer-GPS`

After extracting this archive:

```bash
cd TrackSessionTimer-GPS
git init
git add .
git commit -m "Initial TrackSessionTimer GPS project architecture"
git branch -M main
git remote add origin git@github.com:<owner>/TrackSessionTimer-GPS.git
git push -u origin main
```

If you create the GitHub repository with a README or licence already present, reconcile that first rather than force-pushing over it.

## Then clone as normal

```bash
git clone git@github.com:<owner>/TrackSessionTimer-GPS.git
cd TrackSessionTimer-GPS
```

## Seed labels and issues

Install/authenticate GitHub CLI. Create the standard labels first:

```bash
python tools/create_labels.py
python tools/create_labels.py --execute
```

Then preview the issues:

```bash
python tools/create_issues.py
```

When satisfied:

```bash
python tools/create_issues.py --execute
```


## Recommended first branch

```bash
git checkout -b bootstrap/001-esp-idf
```

Start with issue 001 and do not begin the display port until the base ESP-IDF build and flash path is repeatable.
