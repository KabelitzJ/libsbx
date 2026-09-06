namespace Sbx.Core
{

  /**
   * Per-node scenes::camera field access -- fov/near/far/exposure on whichever camera node a
   * script actually sits on (GetComponent<CameraSettings>()). Distinct from Camera.Main, which is
   * a singleton convenience wrapper always resolving scene.active_camera() natively regardless of
   * which node it's read from -- see Camera's own doc comment.
   */
  public class CameraSettings : Component
  {

    public float FovDegrees
    {
      get
      {
        float value;
        unsafe { InternalCalls.Camera_GetFovDegrees(UUID, &value); }
        return value;
      }
      set
      {
        unsafe { InternalCalls.Camera_SetFovDegrees(UUID, value); }
      }
    }

    public float NearPlane
    {
      get
      {
        float value;
        unsafe { InternalCalls.Camera_GetNearPlane(UUID, &value); }
        return value;
      }
      set
      {
        unsafe { InternalCalls.Camera_SetNearPlane(UUID, value); }
      }
    }

    public float FarPlane
    {
      get
      {
        float value;
        unsafe { InternalCalls.Camera_GetFarPlane(UUID, &value); }
        return value;
      }
      set
      {
        unsafe { InternalCalls.Camera_SetFarPlane(UUID, value); }
      }
    }

    public float Exposure
    {
      get
      {
        float value;
        unsafe { InternalCalls.Camera_GetExposure(UUID, &value); }
        return value;
      }
      set
      {
        unsafe { InternalCalls.Camera_SetExposure(UUID, value); }
      }
    }

  } // class CameraSettings

} // namespace Sbx.Core
