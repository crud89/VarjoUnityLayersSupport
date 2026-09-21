# Varjo Layers API support for Unity 3D

A patch for the Varjo Unity XR plugin native layer to support the layers API.

## What is the Layers API and why do I want to use it in Unity?

Varjo provides a [plugin for Unity3D](https://github.com/varjocom/VarjoUnityXRPlugin) on their own, however, it does not support their [layers API](https://developer.varjo.com/docs/native/rendering-to-varjo-headsets#layers). Layers are a way to express the context into which mixed reality augmentations are rendered. For example, an application may render into chroma-keyed regions or on top of the real-world, but not both at the same time. Layers circumvent this problem, as an application can render some content into a chroma-keying layer and other content into the default AR layer. Unfortunately this is not possible with Unity, due to a lack of support. In Unity one would have to resort back to running multiple applications, which has several downsides:

- Windows throttles the background application, making it run significantly slower.
- Applications don't share the same view matrices, meaning that contents are rendered from slightly different perspectives after head movements.

## How does it work?

Rather than writing an expensive C# wrapper for the different render paths, the Varjo plugin calls into a custom native wrapper (`VarjoXR.dll`), which then communicates with the graphics API, as well as the native Varjo SDK. The Unity plugin doesn't support layered rendering, because this unmanaged wrapper doesn't support it. This project does contain a hook (patching the import address table at runtime) for said native library, which adds the missing support. It cannot be used with Varjo's official plugin, but requires a [custom fork](https://github.com/crud89/VarjoUnityXRPlugin) that implements the Unity side to handle layer targeting. If you only want to use layering in your project, simply install this fork over the official one. Note that if you do not require layered rendering, prefer the official plugin over this fork!

# License

This repository is licensed under the terms of the [MIT license](./LICENSE). Note that this license does only apply to the patch sources provided with this repository and does not extent to the plugin fork!