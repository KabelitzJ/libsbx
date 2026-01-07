using System;
using System.Runtime.InteropServices;

namespace Sbx.Math
{
  [StructLayout(LayoutKind.Sequential, Pack = 4)]
  public struct Quaternion : IEquatable<Quaternion>
  {

    public static readonly Quaternion Identity = new Quaternion(0, 0, 0, 1);

    public float X;
    public float Y;
    public float Z;
    public float W;

    public Vector3 XYZ
    {
      get => new Vector3(X, Y, Z);
      set
      {
        X = value.X;
        Y = value.Y;
        Z = value.Z;
      }
    }
    
    public Quaternion Conjugate
    {
      get => new Quaternion(-X, -Y, -Z, W);
    }

    public Quaternion(float x, float y, float z, float w)
    {
      X = x;
      Y = y;
      Z = z;
      W = w;
    }

    public Quaternion(Vector3 xyz, float w)
    {
      X = xyz.X;
      Y = xyz.Y;
      Z = xyz.Z;
      W = w;
    }

    public Quaternion(Vector3 euler)
    {
      Vector3 cos = Vector3.Cos(euler * 0.5f);
      Vector3 sin = Vector3.Sin(euler * 0.5f);

      X = sin.X * cos.Y * cos.Z - cos.X * sin.Y * sin.Z;
      Y = cos.X * sin.Y * cos.Z + sin.X * cos.Y * sin.Z;
      Z = cos.X * cos.Y * sin.Z - sin.X * sin.Y * cos.Z;
      W = cos.X * cos.Y * cos.Z + sin.X * sin.Y * sin.Z;
    }

		public static Vector3 operator*(Quaternion q, Vector3 v)
		{
			Vector3 qv = new Vector3(q.X, q.Y, q.Z);
			Vector3 uv = Vector3.Cross(qv, v);
      Vector3 uuv = Vector3.Cross(qv, uv);

			return v + ((uv * q.W) + uuv) * 2.0f;
		}

		public static Quaternion operator*(Quaternion a, Quaternion b)
		{
			Quaternion result = new Quaternion();

			result.X = a.W * b.X + a.X * b.W + a.Y * b.Z - a.Z * b.Y;
			result.Y = a.W * b.Y + a.Y * b.W + a.Z * b.X - a.X * b.Z;
			result.Z = a.W * b.Z + a.Z * b.W + a.X * b.Y - a.Y * b.X;
			result.W = a.W * b.W - a.X * b.X - a.Y * b.Y - a.Z * b.Z;

			return result;
		}

    public static float Dot(Quaternion a, Quaternion b)
    {
      return a.X * b.X + a.Y * b.Y + a.Z * b.Z + a.W * b.W;
    }

    public static Quaternion Lerp(Quaternion a, Quaternion b, float t)
    {
      t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);

      // Shortest path
      float dot = Dot(a, b);

      if (dot < 0.0f)
      {
        b.X = -b.X;
        b.Y = -b.Y;
        b.Z = -b.Z;
        b.W = -b.W;
      }

      Quaternion result = new Quaternion(
        a.X + (b.X - a.X) * t,
        a.Y + (b.Y - a.Y) * t,
        a.Z + (b.Z - a.Z) * t,
        a.W + (b.W - a.W) * t
      );

      result.Normalize();

      return result;
    }

    public static Quaternion Slerp(Quaternion a, Quaternion b, float t)
    {
      t = System.Math.Clamp(t, 0.0f, 1.0f);

      float dot = Dot(a, b);

      if (dot < 0.0f)
      {
        b.X = -b.X;
        b.Y = -b.Y;
        b.Z = -b.Z;
        b.W = -b.W;
        dot = -dot;
      }

      // If very close, fallback to lerp
      if (dot > 0.9995f)
      {
        return Lerp(a, b, t);
      }

      float theta0 = MathF.Acos(dot);
      float theta = theta0 * t;

      float sinTheta = MathF.Sin(theta);
      float sinTheta0 = MathF.Sin(theta0);

      float s0 = MathF.Cos(theta) - dot * sinTheta / sinTheta0;
      float s1 = sinTheta / sinTheta0;

      return new Quaternion(
        (a.X * s0) + (b.X * s1),
        (a.Y * s0) + (b.Y * s1),
        (a.Z * s0) + (b.Z * s1),
        (a.W * s0) + (b.W * s1)
      );
    }

    public static Quaternion Euler(Vector3 euler)
    {
      return Euler(euler.X, euler.Y, euler.Z);
    }

    public static Quaternion Euler(float pitch, float yaw, float roll)
    {
      float p = Angle.ToRadians(pitch * 0.5f);
      float y = Angle.ToRadians(yaw * 0.5f);
      float r = Angle.ToRadians(roll * 0.5f);

      float sinp = MathF.Sin(p);
      float cosp = MathF.Cos(p);
      float siny = MathF.Sin(y);
      float cosy = MathF.Cos(y);
      float sinr = MathF.Sin(r);
      float cosr = MathF.Cos(r);

      return new Quaternion(
        cosy * sinp * cosr + siny * cosp * sinr,
        siny * cosp * cosr - cosy * sinp * sinr,
        cosy * cosp * sinr - siny * sinp * cosr,
        cosy * cosp * cosr + siny * sinp * sinr
      );
    }

    public static Quaternion FromAxisAngle(Vector3 axis, float angle)
    {
      axis = axis.Normalized();

      float half = angle * 0.5f;
      float sin = MathF.Sin(half);

      return new Quaternion(
        axis.X * sin,
        axis.Y * sin,
        axis.Z * sin,
        MathF.Cos(half)
      );
    }

    public void Normalize()
    {
      float mag = MathF.Sqrt(X * X + Y * Y + Z * Z + W * W);

      if (mag > 0.0f)
      {
        X /= mag;
        Y /= mag;
        Z /= mag;
        W /= mag;
      }
    }

    public override int GetHashCode()
    {
      return (W, X, Y, Z).GetHashCode();
    }

		public override bool Equals(object obj) 
    {
      return obj is Quaternion other && Equals(other);
    }

		public bool Equals(Quaternion right) 
    {
      return X == right.X && Y == right.Y && Z == right.Z && W == right.W;
    }

		public static bool operator==(Quaternion left, Quaternion right) 
    {
      return left.Equals(right);
    }
    
		public static bool operator!=(Quaternion left, Quaternion right) 
    {
      return !(left == right);
    }
    
  } // struct Quaternion

} // namespace Sbx.Math