using System;
using System.Runtime.InteropServices;

namespace Sbx.Math
{
  [StructLayout(LayoutKind.Sequential, Pack = 4)]
  public struct Quaternion : IEquatable<Quaternion>
  {

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

		public static Vector3 operator *(Quaternion q, Vector3 v)
		{
			Vector3 qv = new Vector3(q.X, q.Y, q.Z);
			Vector3 uv = Vector3.Cross(qv, v);
      Vector3 uuv = Vector3.Cross(qv, uv);

			return v + ((uv * q.W) + uuv) * 2.0f;
		}

		public static Quaternion operator *(Quaternion a, Quaternion b)
		{
			Quaternion result = new Quaternion();

			result.X = a.W * b.X + a.X * b.W + a.Y * b.Z - a.Z * b.Y;
			result.Y = a.W * b.Y + a.Y * b.W + a.Z * b.X - a.X * b.Z;
			result.Z = a.W * b.Z + a.Z * b.W + a.X * b.Y - a.Y * b.X;
			result.W = a.W * b.W - a.X * b.X - a.Y * b.Y - a.Z * b.Z;

			return result;
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

		public static bool operator ==(Quaternion left, Quaternion right) 
    {
      return left.Equals(right);
    }
    
		public static bool operator !=(Quaternion left, Quaternion right) 
    {
      return !(left == right);
    }
    
  } // struct Quaternion

} // namespace Sbx.Math