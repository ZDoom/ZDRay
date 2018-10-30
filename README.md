# ZDRay UDMF properties

<pre>
linedef
{
	lightcolor = &gt;int&lt; (color, default: white)
	lightintensity = &gt;float&lt; (default: 1)
	lightdistance = &gt;float&lt; (default: 0, no light)
}

thing
{
	lightcolor = &gt;int&lt; (color)
	lightintensity = &gt;float&lt; (default: 1)
	lightdistance = &gt;float&lt; (default: 0, no light)
}

sector
{
	lightcolorfloor = &gt;int&lt; (color, default: white)
	lightintensityfloor = &gt;float&lt; (default: 1)
	lightdistancefloor = &gt;float&lt; (default: 0, no light)

	lightcolorceiling = &gt;int&lt; (color, default: white)
	lightintensityceiling = &gt;float&lt; (default: 1)
	lightdistanceceiling = &gt;float&lt; (default: 0, no light)
}
</pre>
