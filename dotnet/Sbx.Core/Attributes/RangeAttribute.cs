using System;

namespace Sbx.Core.Attributes
{

  /// <summary>Shows a float or int field as a slider between Min and Max in the Inspector (unlike ClampValue, which keeps a drag box).</summary>
  [AttributeUsage(AttributeTargets.Field)]
  public class RangeAttribute : Attribute
  {
    public double Min { get; set; }
    public double Max { get; set; }

    public RangeAttribute(double min, double max)
    {
      Min = min;
      Max = max;
    }

  } // class RangeAttribute

} // namespace Sbx.Core.Attributes
