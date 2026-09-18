namespace Sbx.Core
{

  /**
   * Switches, saves, and clears the single active scene from script -- the scripting-side
   * counterpart to the editor's New/Open/Save Scene, and a game's usual "load level"/"restart"/
   * "return to main menu" entry point. Not a Component -- there's exactly one active scene, same
   * as Camera.Main and Physics.
   */
  public static class SceneManager
  {

    /** Replaces the active scene's content with the scene at path (project-relative). Does nothing if path doesn't resolve to a valid scene. */
    public static void Load(string path)
    {
      unsafe { InternalCalls.Scene_Load(path); }
    }

    /** Saves the active scene's current content to path (project-relative), (re-)registering it as a first-class scene asset. */
    public static void Save(string path)
    {
      unsafe { InternalCalls.Scene_Save(path); }
    }

    /** Replaces the active scene with a fresh, empty one (a default camera only). No unsaved-changes guard -- save first with Save() if that matters. */
    public static void New()
    {
      unsafe { InternalCalls.Scene_New(); }
    }

  } // class SceneManager

} // namespace Sbx.Core
