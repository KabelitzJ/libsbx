using System;
using Sbx.Math;

namespace Sbx.Core
{

  public static class Camera
  {

    public static Ray ScreenPointToRay(Vector2 position)
    {
      Ray ray;
      unsafe { InternalCalls.Camera_ScreenPointToRay(&ray, &position); }
      return ray;
    }

    public static Vector3 Position
    {
      get {
        Vector3 position;
        unsafe { InternalCalls.Camera_GetPosition(&position); }
        return position;
      }
      set { unsafe { InternalCalls.Camera_SetPosition(&value); } }
    }
  } // class Camera

} // namespace Sbx.Core