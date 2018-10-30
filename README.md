# ZDRay UDMF properties

linedef
{
	lightcolor = <int> (color, default: white)
	lightintensity = <float> (default: 1)
	lightdistance = <float> (default: 0, no light)
}

thing
{
	lightcolor = <int> (color)
	lightintensity = <float> (default: 1)
	lightdistance = <float> (default: 0, no light)
}

sector
{
	lightcolorfloor = <int> (color, default: white)
	lightintensityfloor = <float> (default: 1)
	lightdistancefloor = <float> (default: 0, no light)

	lightcolorceiling = <int> (color, default: white)
	lightintensityceiling = <float> (default: 1)
	lightdistanceceiling = <float> (default: 0, no light)
}
