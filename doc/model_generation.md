## Generating the model

The model is generated by `calcmysky` utility from a text file, whose name normally has `.atmo` suffix. The command
```
calcmysky description.atmo --out-dir output-directory
```
will use the input file `description.atmo` and output the model to a directory named `output-directory`.

This utility has some options that can be listed by running it with `--help` option.

### Format of model description file

Model description files consist of entries that represent key-value pairs. Empty lines between entries are ignored, and "#" character starts comments. Single-line entries can also be followed by a comment in the same line.

Any text on a line before the first colon character ":" is considered to be key. Depending on the key, corresponding value can be one of the following:
 * an integral value, e.g. 123;
 * a fractional value, e.g. 123.8767;
 * a dimensionful quantity, e.g. 33.2 km;
 * a list of numbers, e.g. 1.23,6.3245,-7.948,5;
 * a range of wavelengths, e.g. min=360nm,max=830nm,count=16;
 * a section.

A section begins on the next line after the key, the colon character remains on the line with the key.

Top-level sections are bounded by an opening brace "{" and a closing brace "}". Nested section, which normally are code snippets, are bounded by triple backticks "```", similarly to fenced code blocks in Markdown.

The `examples` directory in the source tree has a few `*.atmo` files serving as examples. Let's walk through one of them, `sample.atmo`.

```
atmosphere height: 120 km
```
Atmosphere height is the maximum altitude at which density of some constituents of the atmosphere is considered nonzero. The sphere of all the points at this altitude is called TOA, i.e. top of atmosphere. All the textures span altitudes from 0 to atmosphere height.
Valid values for this entry are length quantities from 1&nbsp;m to 1&nbsp;Mm.

```
transmittance texture size for VZA: 256
transmittance texture size for altitude: 64
```
Transmittance texture holds a map of percentage of light that's transmitted through the atmosphere from the TOA to the camera, given altitude of a camera and view zenith angle (VZA) of this camera. The entries above control the resolution of the transmittance texture in each of these dimensions.

```
irradiance texture size for SZA: 64
irradiance texture size for altitude: 16
```
Irradiance texture contains values of irradiance of a horizontal surface at the point with given altitude and zenith angle of the Sun (SZA). The entries above control the resolution of the irradiance texture in each of these dimensions.

```
scattering texture size for VZA: 128
scattering texture size for dot(view,sun): 16
scattering texture size for SZA: 128
scattering texture size for altitude: 64
```
Scattering textures are the main textures that contain the actual radiance or luminance from given direction as seen by a camera at the given altitude, with the Sun being at a given zenith angle.

VZA, SZA and altitude here are similar to the corresponding parameters in irradiance and transmittance textures.

The `dot(view,sun)` parameter determines cosine of the angular distance between the direction from the camera to the Sun and the direction of view of the camera.

For technical reasons the texture size for VZA here must be even.

```
eclipsed scattering texture size for relative azimuth: 32
eclipsed scattering texture size for VZA: 128
```
When solar eclipse is simulated, corresponding first-order scattering texture is computed on the fly during rendering, for the current altitude of the camera and the current positions of the Sun and the Moon. This is unlike the case of non-eclipsed calculations, where such radiance is precomputed for all altitudes and solar elevations.

Relative azimuth is the azimuth of the view direction relative to the Sun. VZA is view zenith angle, as described for transmittance texture.

```
eclipsed double scattering texture size for relative azimuth: 16
eclipsed double scattering texture size for VZA: 128
eclipsed double scattering texture size for SZA: 16
```
When solar eclipse is simulated, it's only simulated for two scattering orders, because going further is prohibitively slow. Even second-order scattering is quite slow on many GPUs. These entries control precomputation of a texture that will hold second-order scattering radiance for the given view zenith angle, solar elevation, and azimuth of view relative to the Sun.

```
eclipsed double scattering number of azimuth pairs to sample: 2
eclipsed double scattering number of elevation pairs to sample: 10
```
Because second-order scattering is so slow to compute, radiance is sampled on a very sparse grid of points. Several circles at different azimuths are sampled at multiple elevations, with an optimized distribution of samples. The samples are taken in pairs, e.g. if one sampling direction is forward, one more will be backward.

The more samples one takes, the higher the resolution of the final second-order scattering layer.

```
light pollution texture size for VZA: 128
light pollution texture size for altitude: 64
```
Light pollution is simulated with the assumption that the whole Earth glows uniformly with the given luminance (which is controlled at runtime). This makes the radiance quite symmetric, depending only on altitude of the camera and zenith angle of its view direction. These entries control resolution in these parameters.


<span style="background-color: red;">TODO</span>: finish this section
