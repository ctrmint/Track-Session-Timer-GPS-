# Planning snapshots

GitHub issues and milestones are the authoritative development plan:

- Repository issues: https://github.com/ctrmint/Track-Session-Timer-GPS-/issues
- Milestones: https://github.com/ctrmint/Track-Session-Timer-GPS-/milestones

The CSV files are a reviewable bootstrap/export snapshot. Run `make issue-preview`
or `make label-preview` to inspect the corresponding GitHub CLI commands. Execute
mode is idempotent by title for issues and uses `--force` for labels, but native
sub-issue/dependency relationships still need the GitHub API when seeding a new repo.
