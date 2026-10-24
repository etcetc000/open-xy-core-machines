# Instructions for agents

This repository holds Open-XY's own machines. It is not the SDK, and it is not
where a new user's machine goes: a new machine starts from the
[machine template](https://github.com/etcetc000/open-xy-machine-template) in a
repository of its own.

- Clone the SDK beside this repository (`git clone
  https://github.com/etcetc000/open-xy ../open-xy`, default branch `staging`) and
  run its commands from here, for example
  `python ../open-xy/sdk.py test --machine machines/<id>`.
- Change a machine only inside its own folder. Machine code uses only the public
  `<opxy/...>` headers: no assembly, platform conditionals, OS headers or raw
  addresses, and audio callbacks never allocate, block, log or do I/O. SDK gaps
  are separate pull requests to the SDK's `staging` branch.
- Keep vendored code under `vendor/` byte-for-byte, with its licence.
- A release is a tag here, then a pull request against the SDK's `staging` that
  changes only `catalog/machines.json` to pin the new commit.
- Device operations need the user's explicit instruction each time.
