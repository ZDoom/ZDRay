
# ZDRay baking utility for GZDoom

ZDRay is a node and lightmap generator. It is based on zdbsp for the node generation and dlight for the lightmap generation.

Special thanks to Randi Heit, Samuel Villarreal, Christoph Oelckers and anyone else involved in creating or maintaining those tools.

## ZDRay UDMF properties

<pre>
linedef
{
	lightcolor = &lt;int&gt; (color, default: white)
	lightintensity = &lt;float&gt; (default: 1)
	lightdistance = &lt;float&gt; (default: 0, no light)
}

thing
{
	lightcolor = &lt;int&gt; (color)
	lightintensity = &lt;float&gt; (default: 1)
	lightdistance = &lt;float&gt; (default: 0, no light)
	lightinnerangle = &lt;float&gt; (default: 180)
	lightouterangle = &lt;float&gt; (default: 180)
}

sector
{
	lightcolorfloor = &lt;int&gt; (color, default: white)
	lightintensityfloor = &lt;float&gt; (default: 1)
	lightdistancefloor = &lt;float&gt; (default: 0, no light)

	lightcolorceiling = &lt;int&gt; (color, default: white)
	lightintensityceiling = &lt;float&gt; (default: 1)
	lightdistanceceiling = &lt;float&gt; (default: 0, no light)
}
</pre>
