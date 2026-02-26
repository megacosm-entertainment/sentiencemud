# Branch Protection + CI Required Checks Checklist

Use this checklist to ensure protected branches cannot merge unless bootstrap CI tests pass.

## 1) Open branch protection settings

1. Go to repository Settings.
2. Go to Branches.
3. Under Branch protection rules, create or edit rules for each protected branch:
   - master
   - v2_testport
   - legacy_server
   - olc_blueprints
   - feature/widevnum-migration

## 2) Enable pull request protections

Enable:

- Require a pull request before merging
- Require approvals (team preference)

Optional but recommended:

- Dismiss stale pull request approvals when new commits are pushed
- Require review from Code Owners (if CODEOWNERS is in use)

## 3) Require CI status checks

Enable:

- Require status checks to pass before merging

Select required check:

- CI Bootstrap Tests / Bootstrap-root regression tests

Optional but recommended:

- Require branches to be up to date before merging

## 4) Validate merge queue support (optional)

If using merge queue:

- Keep merge_group trigger enabled in .github/workflows/ci-bootstrap-tests.yml
- Ensure the same required check name is used by merge queue runs

## 5) Configure Discord webhooks (optional notifications)

Set repository secrets:

- GHA_CI_TESTS_DISCORD_WEBHOOK (CI result notifications)
- GHA_MERGES_DISCORD_WEBHOOK (generic merge/push notifications)
- GHA_LEGACY_DISCORD_WEBHOOK (branch-specific override for legacy_server)
- GHA_20_DISCORD_WEBHOOK (branch-specific override for v2_testport)

Related workflows:

- .github/workflows/ci-bootstrap-tests.yml
- .github/workflows/ci-bootstrap-discord.yml
- .github/workflows/merges.yml

## 6) Smoke test the policy

1. Open a PR to a protected branch.
2. Confirm CI Bootstrap Tests workflow runs.
3. Confirm required check appears and must pass.
4. Confirm merge is blocked while check is failing.
5. Confirm merge is allowed once check passes.

## 7) Ongoing maintenance

When adding/removing protected branches:

1. Update branch protection rules.
2. Update workflow triggers in .github/workflows/ci-bootstrap-tests.yml and .github/workflows/merges.yml.
3. Re-verify required check binding in branch protection UI.
