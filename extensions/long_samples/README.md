# Longer samples

A settings extension: only `extension.json`, declaring the SDK's `long_samples`
capability. It links no code.

The stock loader stops a sample at 882,000 frames (20 seconds at 44.1 kHz). With
this extension a file loads up to 1,048,575 frames (23.78 seconds), the most a
stock sample object holds, on every sampler and drum track. Recording still stops
at 20 seconds.

Select it in the builder's Extensions step, or apply it to an existing OS image:

```sh
python ../open-xy/sdk.py settings apply --image os.ldr --extension extensions/long_samples --out ../build-long
```

For an open-xy OS built with another SDK, add `--stock-os <stock os.ldr>`.

Needs the SDK's `long_samples` capability.
