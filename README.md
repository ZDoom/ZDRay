
# ZDRay baking utility for GZDoom

ZDRay is a node and lightmap generator. It is based on zdbsp for the node generation and dlight for the lightmap generation.

Special thanks to Randi Heit, Samuel Villarreal, Christoph Oelckers and anyone else involved in creating or maintaining those tools.

## ZDRay UDMF properties

<pre>
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

thing // Sunlight (sunlight properties for the map)
{
	type = 9876;
	suncolor = &lt;int&gt; (color)
	sundirx = &lt;float&gt; (X direction for the sun)
	sundiry = &lt;float&gt; (Y direction for the sun)
	sundirz = &lt;float&gt; (Z direction for the sun)
}

linedef // Line surface emitting
{
	lightcolor = &lt;int&gt; (color, default: white)
	lightintensity = &lt;float&gt; (default: 1)
	lightdistance = &lt;float&gt; (default: 0, no light)
}

sector // Sector planes emitting light
{
	lightcolorfloor = &lt;int&gt; (color, default: white)
	lightintensityfloor = &lt;float&gt; (default: 1)
	lightdistancefloor = &lt;float&gt; (default: 0, no light)

	lightcolorceiling = &lt;int&gt; (color, default: white)
	lightintensityceiling = &lt;float&gt; (default: 1)
	lightdistanceceiling = &lt;float&gt; (default: 0, no light)
}
</pre>
