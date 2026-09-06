using System;
using Sbx.Math;

namespace Sbx.Core
{

  /**
   * Always resolves scenes::scene::active_camera() natively -- a singleton convenience wrapper,
   * not a per-node component (see CameraSettings for that: fov_degrees/near_plane/far_plane/
   * exposure on whichever camera node a script actually sits on, via GetComponent<CameraSettings>()).
   */
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
        unsafe { InternalCalls.Camera_MainGetPosition(&position); }
        return position;
      }
      set { unsafe { InternalCalls.Camera_MainSetPosition(&value); } }
    }

    public Quaternion Rotation
    {
      get {
        Quaternion rotation;
        unsafe { InternalCalls.Camera_MainGetRotation(&rotation); }
        return rotation;
      }
      set { unsafe { InternalCalls.Camera_MainSetRotation(&value); } }
    }

    public Vector3 Forward
    {
      get {
        Vector3 forward;
        unsafe { InternalCalls.Camera_MainGetForward(&forward); }
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
        unsafe { InternalCalls.Camera_MainGetRight(&forward); }
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
        unsafe { InternalCalls.Camera_MainGetUp(&up); }
        return up;
      }
    }

    public Vector2 Viewport
    {
      get {
        Vector2 viewport;
        unsafe { InternalCalls.Camera_GetViewport(&viewport); }
        return viewport;
      }
    }

  } // class Camera

} // namespace Sbx.Core
