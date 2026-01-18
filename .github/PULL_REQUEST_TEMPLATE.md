### Description

<!--- Clearly describe the changes introduced by this PR. What was fixed, added, or updated? --->

### Reproduction Steps and Savefile (if available)

<!--- Provide detailed steps to reproduce the issue. Include a savefile if applicable, preferrably for Sonora --->

### Screenshots

<!--- If the PR includes visual changes, add screenshots here. --->


<!--- 

**Important Notes** 

- *Preview builds are not available for pull requests from forked repositories.* To enable preview builds, submit the PR from a branch within this repository. 

- *Formatting issues? You have two options:*
1. **Fix formatting automatically** using the following Docker command: *(If you're using Windows, remove `--user $(id -u):$(id -g)`)* 
```bash
docker run --rm \
  -v $(pwd):/app --workdir /app  \
  --user $(id -u):$(id -g) silkeh/clang:18 \
  bash -c 'find src -type f -name \*.cc -o -name \*.h | xargs clang-format -i'
```
2. **Skip formatting check** by adding the `skip-formatting` label to this PR.

--->

