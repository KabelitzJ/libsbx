using System;
using Sbx.Math;

namespace Sbx.Core
{

  public class Camera
  {

    public static Camera Main = new Camera();

    public Ray ScreenPointToRay(Vector2 position)
    {
      Ray ray;
      unsafe { InternalCalls.Camera_ScreenPointToRay(&ray, &position); }
      return ray;
    }

    public Vector3 Position
    {
      get {
        Vector3 position;
        unsafe { InternalCalls.Camera_GetPosition(&position); }
        return position;
      }
      set { unsafe { InternalCalls.Camera_SetPosition(&value); } }
    }

    public Quaternion Rotation
    {
      get {
        Quaternion rotation;
        unsafe { InternalCalls.Camera_GetRotation(&rotation); }
        return rotation;
      }
      set { unsafe { InternalCalls.Camera_SetRotation(&value); } }
    }

    public Vector3 Forward
    {
      get {
        Vector3 forward;
        unsafe { InternalCalls.Camera_GetForward(&forward); }
        return forward;
      }
    }

    public Vector3 FlatForward
    {
      get
      {
        var forward = Forward;
        forward.Y = 0.0f;
        return forward.Normalized();
      }
    }

    public Vector3 Right
    {
      get {
        Vector3 forward;
        unsafe { InternalCalls.Camera_GetRight(&forward); }
        return forward;
      }
    }

    public Vector3 FlatRight
    {
      get
      {
        var right = Right;
        right.Y = 0.0f;
        return right.Normalized();
      }
    }

    public Vector3 Up
    {
      get {
        Vector3 up;
        unsafe { InternalCalls.Camera_GetUp(&up); }
        return up;
      }
    }

  } // class Camera

} // namespace Sbx.Core