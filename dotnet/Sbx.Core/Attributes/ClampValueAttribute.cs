using System;

namespace Sbx.Core.Attributes
{

	[AttributeUsage(AttributeTargets.Field)]
	public class ClampValueAttribute : Attribute
	{
		public double Min { get; set; }
    public double Max { get; set; }

    public ClampValueAttribute()
    {
      
    }

    public ClampValueAttribute(double min, double max)
    {
      Min = min;
      Max = max;
    }

	} // public class ClampValueAttribute

} // namespace Sbx.Attributes
