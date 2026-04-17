using System.Runtime.InteropServices;

namespace Sbx.Math
{
  [StructLayout(LayoutKind.Sequential, Pack = 4)]
  public struct Vector2
  {

    public static Vector2 Zero = new Vector2(0, 0);
    public static Vector2 One = new Vector2(1, 1);
    public static Vector2 Right = new Vector2(1, 0);
    public static Vector2 Left = new Vector2(-1, 0);
    public static Vector2 Up = new Vector2(0, 1);
    public static Vector2 Down = new Vector2(0, -1);

    public float X;
    public float Y;

    public Vector2(float x, float y)
    {
      X = x;
      Y = y;
    }

    public Vector2(float scalar)
    : this(scalar, scalar)
    { }

    public Vector2(Vector3 vector)
    : this(vector.X, vector.Y) 
    { }

    public float LengthSquared()
    {
      return X * X + Y * Y;
    }

    public float Length()
    {
      return MathF.Sqrt(LengthSquared());
    }

    public Vector2 Normalized()
    {
      float lenSq = LengthSquared();

      if (lenSq < 1e-6f)
      {
        return new Vector2(X, Y);
      }

      float invLen = 1.0f / MathF.Sqrt(lenSq);

      return new Vector2(X * invLen, Y * invLen);
    }

    public float Distance(Vector2 other)
    {
      return MathF.Sqrt(MathF.Pow(other.X - X, 2) + MathF.Pow(other.Y - Y, 2));
    }

    public static float Distance(Vector2 p1, Vector2 p2)
    {
      return MathF.Sqrt(MathF.Pow(p2.X - p1.X, 2) + MathF.Pow(p2.Y - p1.Y, 2));
    }

    //Lerps from p1 to p2
    public static Vector2 Lerp(Vector2 p1, Vector2 p2, float maxDistanceDelta)
    {
      if (maxDistanceDelta < 0.0f)
      {
        return p1;
      }

      if (maxDistanceDelta > 1.0f)
      {
        return p2;
      }

      return p1 + ((p2 - p1) * maxDistanceDelta);
    }

    public static float Dot(Vector2 lhs, Vector2 rhs) {
      return lhs.X * rhs.X + lhs.Y * rhs.Y; 
    }

    public static Vector2 operator *(Vector2 left, float scalar)
    {
      return new Vector2(left.X * scalar, left.Y * scalar);
    }

    public static Vector2 operator *(float scalar, Vector2 right) 
    {
      return  new Vector2(scalar * right.X, scalar * right.Y);
    }
    
    public static Vector2 operator *(Vector2 left, Vector2 right) 
    {
      return  new Vector2(left.X * right.X, left.Y * right.Y);
    }
    
    public static Vector2 operator /(Vector2 left, Vector2 right) 
    {
      return  new Vector2(left.X / right.X, left.Y / right.Y);
    }
    
    public static Vector2 operator /(Vector2 left, float scalar) 
    {
      return  new Vector2(left.X / scalar, left.Y / scalar);
    }
    
    public static Vector2 operator /(float scalar, Vector2 right) 
    {
      return  new Vector2(scalar / right.X, scalar / right.Y);
    }
    
    public static Vector2 operator +(Vector2 left, Vector2 right) 
    {
      return  new Vector2(left.X + right.X, left.Y + right.Y);
    }
    
    public static Vector2 operator +(Vector2 left, float right) 
    {
      return  new Vector2(left.X + right, left.Y + right);
    }
    
    public static Vector2 operator -(Vector2 left, Vector2 right) 
    {
      return  new Vector2(left.X - right.X, left.Y - right.Y);
    }
    
    public static Vector2 operator -(Vector2 left, float right) 
    {
      return  new Vector2(left.X - right, left.Y - right);
    }

    public static Vector2 operator -(Vector2 vector)
    {
      return new Vector2(-vector.X, -vector.Y);
    }
    
    public override bool Equals(object obj) 
    {
      return  obj is Vector2 other && Equals(other);
    }
    
    public bool Equals(Vector2 right) 
    {
      return  X == right.X && Y == right.Y;
    }
    
    public override int GetHashCode() 
    {
      return  (X, Y).GetHashCode();
    }
    
    public static bool operator ==(Vector2 left, Vector2 right) 
    {
      return  left.Equals(right);
    }
    
    public static bool operator !=(Vector2 left, Vector2 right) 
    {
      return  !(left == right);
    }

    public override string ToString()
    {
      return string.Format("Vector2[{0}, {1}]", X, Y);
    }
    
  } // struct Vector2
  
} // namespace Sbx.Math