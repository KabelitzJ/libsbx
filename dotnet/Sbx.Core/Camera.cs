using System;
using Sbx.Core.Math;

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

    /**
     * Inverse of ScreenPointToRay. Returns false (screenPosition left at default) if worldPosition
     * sits at or behind the camera -- there's no well-defined screen point for it.
     */
    public bool WorldToScreenPoint(Vector3 worldPosition, out Vector2 screenPosition)
    {
      Vector2 position;
      bool didProject;
      unsafe { didProject = InternalCalls.Camera_WorldToScreenPoint(&worldPosition, &position); }
      screenPosition = position;
      return didProject;
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

    /**
     * Where the viewport's top-left corner sits within the window -- always zero in a standalone
     * build, nonzero in the editor (the game view is a docked panel inset within the window).
     * Input.MousePosition() is raw window space, but RectTransform/canvas coordinates are viewport
     * space -- subtract this before feeding a mouse position into UI positioning.
     */
    public Vector2 ViewportOffset
    {
      get {
        Vector2 offset;
        unsafe { InternalCalls.Camera_GetViewportOffset(&offset); }
        return offset;
      }
    }

  } // class Camera

} // namespace Sbx.Core
