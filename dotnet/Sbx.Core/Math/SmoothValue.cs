using System;

namespace Sbx.Math
{
  public enum SmoothingMode : byte
  {
    Linear,
    Proportional
  } // public enum SmoothingMode

  public class SmoothValue
{
    public float Current { get; private set; }
    public float Target  { get; private set; }

    private readonly SmoothingMode mode;

    public SmoothValue(float value, SmoothingMode mode)
    {
      this.mode = mode;
      Current = value;
      Target = value;
    }

    // Assignment-style operator
    public static SmoothValue operator +(SmoothValue value, float v)
    {
      value.Target += v;
      return value;
    }

    public static SmoothValue operator -(SmoothValue value, float v)
    {
      value.Target -= v;
      return value;
    }

    public static SmoothValue operator *(SmoothValue value, float v)
    {
      value.Target *= v;
      return value;
    }

    public static SmoothValue operator /(SmoothValue value, float v)
    {
      value.Target /= v;
      return value;
    }

    public static SmoothValue Clamp(SmoothValue value, float min, float max)
    {
      return new SmoothValue(value.Current, value.mode)
      {
        Target = System.Math.Clamp(value.Target, min, max)
      };
    }

    public float Value()
    {
      return Current;
    }

    public static implicit operator float(SmoothValue value)
    {
      return value.Current;
    }

    public void Set(float value)
    {
      Target = value;
    }

    public void Update(float deltaTime, float baseSpeed)
    {
      float diff = Target - Current;

      if (MathF.Abs(diff) <= 0.000001f)
      {
        Current = Target;
        return;
      }

      float step = ComputeStep(diff, baseSpeed, deltaTime);

      if (MathF.Abs(step) >= MathF.Abs(diff))
      {
        Current = Target;
      }
      else
      {
        Current += step;
      }
    }

    private float ComputeStep(float diff, float baseSpeed, float deltaTime)
    {
      return mode switch
      {
        SmoothingMode.Linear => MathF.Sign(diff) * baseSpeed * deltaTime,
        SmoothingMode.Proportional => diff * baseSpeed * deltaTime,
        _ => 0f
      };
    }

  } // class SmoothValue

  // Convenience types
  public class LinearSmoothValue : SmoothValue
  {

    public LinearSmoothValue(float value)
    : base(value, SmoothingMode.Linear) { }

  } // class LinearSmoothValue

  public class ProportionalSmoothValue : SmoothValue
  {

    public ProportionalSmoothValue(float value)
    : base(value, SmoothingMode.Proportional) { }

  } // class ProportionalSmoothValue

} // namespace Sbx.Math
