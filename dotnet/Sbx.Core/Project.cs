namespace Sbx.Core
{
  public static class Project
  {
    /** Absolute path of the active project's assets directory -- for reading a script's own data files (System.IO) that aren't engine assets. */
    public static string AssetsDirectory
    {
      get { unsafe { return InternalCalls.Project_GetAssetsDirectory(); } }
    }
  } // class Project
} // namespace Sbx.Core
