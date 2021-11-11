
# ZDRay baking utility for GZDoom

ZDRay is a node and lightmap generator for GZDoom. ZDRay is intended as a drop-in replacement for zdbsp, with the additional feature
that it can also bake lights. Once ZDRay has processed the level WAD it is ready to be used by GZDoom.

ZDRay is based on zdbsp for the node generation and originally used dlight for the lightmap generation. Special thanks to Randi Heit,
Samuel Villarreal, Christoph Oelckers and anyone else involved in creating or maintaining those tools.

The ray tracing code has been completely rewritten since. It now supports bounces and can do the ray tracing on the GPU.

## ZDRay UDMF properties

<pre>
thing // ZDRayInfo (zdray properties for the map)
{
	type = 9890;
	suncolor = &lt;int&gt; (color)
	sundirx = &lt;float&gt; (X direction for the sun)
	sundiry = &lt;float&gt; (Y direction for the sun)
	sundirz = &lt;float&gt; (Z direction for the sun)
	sampledistance = &lt;int&gt; (default: 8, map units each lightmap texel covers, must be in powers of two)
	bounces = &lt;int&gt; (default: 1, how many times light bounces off walls)
	gridsize = &lt;float&gt; (default: 32, grid density for the automatic light probes)
}

thing // StaticLight (point or spot light to be baked into the lightmap)
{
	lightcolor = &lt;int&gt; (color)
	lightintensity = &lt;float&gt; (default: 1)
	lightdistance = &lt;float&gt; (default: 0, no light)
	lightinnerangle = &lt;float&gt; (default: 180)
	lightouterangle = &lt;float&gt; (default: 180)
}

thing // LightProbe (light sampling point for actors)
{
	type = 9875;
}

linedef // Line emissive surface
{
	lightcolor = &lt;int&gt; (color, default: white)
	lightintensity = &lt;float&gt; (default: 1)
	lightdistance = &lt;float&gt; (default: 0, no light)
}

sector // Sector plane emissive surface
{
	lightcolorfloor = &lt;int&gt; (color, default: white)
	lightintensityfloor = &lt;float&gt; (default: 1)
	lightdistancefloor = &lt;float&gt; (default: 0, no light)

	lightcolorceiling = &lt;int&gt; (color, default: white)
	lightintensityceiling = &lt;float&gt; (default: 1)
	lightdistanceceiling = &lt;float&gt; (default: 0, no light)
}
</pre>
