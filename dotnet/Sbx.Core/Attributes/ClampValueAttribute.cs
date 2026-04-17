using System;

namespace Sbx.Attributes
{

	[AttributeUsage(AttributeTargets.Field)]
	public class ClampValueAttribute : Attribute
	{
		public readonly float Min;
		public readonly float Max;

    public ClampValueAttribute(float min, float max)
    {
      Min = min;
      Max = max;
    }

	} // public class ClampValueAttribute

} // namespace Sbx.Attributes