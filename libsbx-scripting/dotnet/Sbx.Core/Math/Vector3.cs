using System.Runtime.InteropServices;

namespace Sbx.Math
{

  [StructLayout(LayoutKind.Sequential, Pack = 4)]
  public struct Vector3 : IEquatable<Vector3>
  {

    public static Vector3 Zero = new Vector3(0, 0, 0);
    public static Vector3 One = new Vector3(1, 1, 1);
    public static Vector3 Forward = new Vector3(0, 0, -1);
    public static Vector3 Back = new Vector3(0, 0, 1);
    public static Vector3 Right = new Vector3(1, 0, 0);
    public static Vector3 Left = new Vector3(-1, 0, 0);
    public static Vector3 Up = new Vector3(0, 1, 0);
    public static Vector3 Down = new Vector3(0, -1, 0);

    public static Vector3 Inifinity = new Vector3(float.PositiveInfinity);

    public float X;
    public float Y;
    public float Z;

    public Vector3(float x, float y, float z)
    {
      X = x;
      Y = y;
      Z = z;
    }

    public Vector3(float scalar)
    : this(scalar, scalar, scalar)
    { }

    public Vector3(float x, Vector2 yz)
    : this(x, yz.X, yz.Y)
    { }

    public Vector3(Vector2 xy, float z = 0.0f)
    : this(xy.X, xy.Y, z)
    { }

    public static Vector3 operator*(Vector3 left, float scalar)
    {
      return new Vector3(left.X * scalar, left.Y * scalar, left.Z * scalar);
    }

    public static Vector3 operator*(float scalar, Vector3 right)
    {
      return new Vector3(scalar * right.X, scalar * right.Y, scalar * right.Z);
    }

    public static Vector3 operator*(Vector3 left, Vector3 right)
    {
      return new Vector3(left.X * right.X, left.Y * right.Y, left.Z * right.Z);
    }

		public static Vector3 operator/(Vector3 left, Vector3 right) 
    {
      return new Vector3(left.X / right.X, left.Y / right.Y, left.Z / right.Z);
    }
    
		public static Vector3 operator/(Vector3 left, float scalar) 
    {
      return new Vector3(left.X / scalar, left.Y / scalar, left.Z / scalar);
    }
    
		public static Vector3 operator/(float scalar, Vector3 right) 
    {
      return new Vector3(scalar/ right.X, scalar/ right.Y, scalar/ right.Z);
    }
    
		public static Vector3 operator+(Vector3 left, Vector3 right) 
    {
      return new Vector3(left.X + right.X, left.Y + right.Y, left.Z + right.Z);
    }
    
		public static Vector3 operator+(Vector3 left, float right) 
    {
      return new Vector3(left.X + right, left.Y + right, left.Z + right);
    }
    
		public static Vector3 operator-(Vector3 left, Vector3 right) 
    {
      return new Vector3(left.X - right.X, left.Y - right.Y, left.Z - right.Z);
    }
    
		public static Vector3 operator-(Vector3 left, float right) 
    {
      return new Vector3(left.X - right, left.Y - right, left.Z - right);
    }

    public static Vector3 operator-(Vector3 vector)
    {
      return new Vector3(-vector.X, -vector.Y, -vector.Z);
    }

    public override bool Equals(object obj)
    {
      return obj is Vector3 other && Equals(other);
    }

    public bool Equals(Vector3 other) {
      return X == other.X && Y == other.Y && Z == other.Z;
    }

    public override int GetHashCode()
    {
      return (X, Y, Z).GetHashCode();
    }

    public static bool operator==(Vector3 left, Vector3 right)
    {
      return left.Equals(right);
    }

    public static bool operator!=(Vector3 left, Vector3 right)
    {
      return !(left == right);
    }

    public float LengthSquared()
    {
      return X * X + Y * Y + Z * Z;
    }

    public float Length()
    {
      return MathF.Sqrt(LengthSquared());
    }

    public Vector3 Normalized()
		{
			float length = Length();

			return length > 0.0f ? Create(this, v => v / length) : new Vector3(X, Y, Z);
		}

		public void Normalize()
		{
			float length = Length();

			if (length > 0.0f)
      {
				Apply(v => v / length);
      }
		}

    public float Distance(Vector3 other)
		{
			return MathF.Sqrt(MathF.Pow(other.X - X, 2) + MathF.Pow(other.Y - Y, 2) + MathF.Pow(other.Z - Z, 2));
		}

		public static float Distance(Vector3 lhs, Vector3 rhs)
		{
			return MathF.Sqrt(MathF.Pow(rhs.X - lhs.X, 2) + MathF.Pow(rhs.Y - lhs.Y, 2) + MathF.Pow(rhs.Z - lhs.Z, 2));
		}

    public static float Dot(Vector3 a, Vector3 b)
    {
      return a.X * b.X + a.Y * b.Y + a.Z * b.Z;
    }

    public static Vector3 Cross(Vector3 a, Vector3 b)
		{
			return new Vector3(
				a.Y * b.Z - b.Y * a.Z,
				a.Z * b.X - b.Z * a.X,
				a.X * b.Y - b.X * a.Y
			);
		}

    public static Vector3 Cos(Vector3 vector)
    {
      return Create(vector, MathF.Cos);
    }

    public static Vector3 Sin(Vector3 vector)
    {
      return Create(vector, MathF.Sin);
    }

    public static Vector3 Lerp(Vector3 a, Vector3 b, float t)
    {
      t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);

      return a + (b - a) * t;
    }

    public override string ToString()
    {
      return string.Format("Vector3[{0}, {1}, {2}]", X, Y, Z);
    }

    public void Apply(Func<float, float> function)
		{
			X = function(X);
			Y = function(Y);
			Z = function(Z);
		}

    public static Vector3 Create(Vector3 vector, Func<float, float> function)
    {
      return new Vector3(function(vector.X), function(vector.Y), function(vector.Z));
    }

  } // struct Vector3

} // namespace Sbx.Math