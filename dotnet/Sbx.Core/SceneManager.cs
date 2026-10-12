namespace Sbx.Core
{

  public enum LoadSceneMode : uint
  {
    /** Unloads every loaded scene first, except nodes marked DontDestroyOnLoad. */
    Single = 0,
    /** Loads alongside the scenes already loaded. */
    Additive = 1,
  }

  /**
   * Loads, unloads and saves scenes from script, like Unity's SceneManager. Loads take effect at the
   * start of the next frame, so the calling script finishes its Update first. In a built game only
   * scenes in the project's scene list (Project Settings > Scenes) can be loaded. Scene events
   * arrive as Behavior.OnSceneLoaded / OnSceneUnloaded / OnActiveSceneChanged.
   */
  public static class SceneManager
  {

    /** How many enabled scenes the project's scene list has; build indices run from 0 to this minus 1. */
    public static int SceneCountInBuildSettings
    {
      get { unsafe { return InternalCalls.Scene_CountInBuildSettings(); } }
    }

    /** Loaded scenes, not counting the DontDestroyOnLoad scene. */
    public static int SceneCount
    {
      get { unsafe { return InternalCalls.Scene_GetCount(); } }
    }

    /** Loads a scene by its name in the scene list (file name without extension, case-insensitive) or by its path, e.g. "levels/forest". */
    public static void LoadScene(string sceneNameOrPath, LoadSceneMode mode = LoadSceneMode.Single)
    {
      unsafe { InternalCalls.Scene_LoadByName(sceneNameOrPath, (uint)mode, null); }
    }

    /** Loads the scene at buildIndex in the project's scene list. */
    public static void LoadScene(int buildIndex, LoadSceneMode mode = LoadSceneMode.Single)
    {
      unsafe { InternalCalls.Scene_LoadByIndex(buildIndex, (uint)mode, null); }
    }

    /**
     * Loads in the background: the file is parsed off the main thread and its assets are loaded before the scene
     * activates at the start of a frame. Async loads activate in the order started.
     */
    public static AsyncOperation LoadSceneAsync(string sceneNameOrPath, LoadSceneMode mode = LoadSceneMode.Single)
    {
      uint handle;
      uint operation;
      unsafe { operation = InternalCalls.Scene_LoadAsyncByName(sceneNameOrPath, (uint)mode, &handle); }
      return new AsyncOperation(operation, handle);
    }

    public static AsyncOperation LoadSceneAsync(int buildIndex, LoadSceneMode mode = LoadSceneMode.Single)
    {
      uint handle;
      uint operation;
      unsafe { operation = InternalCalls.Scene_LoadAsyncByIndex(buildIndex, (uint)mode, &handle); }
      return new AsyncOperation(operation, handle);
    }

    /** Unloads a loaded scene next frame, running OnDestroy on its scripts. The last loaded scene can't be unloaded. */
    public static AsyncOperation UnloadSceneAsync(Scene scene)
    {
      unsafe { return new AsyncOperation(InternalCalls.Scene_Unload(scene.Handle), scene.Handle); }
    }

    public static AsyncOperation UnloadSceneAsync(string sceneNameOrPath)
    {
      var scene = GetSceneByName(sceneNameOrPath);
      return UnloadSceneAsync(scene.IsValid ? scene : GetSceneByPath(sceneNameOrPath));
    }

    public static AsyncOperation UnloadSceneAsync(int buildIndex)
    {
      return UnloadSceneAsync(GetSceneByBuildIndex(buildIndex));
    }

    /** The scene new nodes are created in. */
    public static Scene GetActiveScene()
    {
      unsafe { return new Scene(InternalCalls.Scene_GetActive()); }
    }

    /** Makes a loaded scene the one new nodes are created in. False if it isn't loaded. */
    public static bool SetActiveScene(Scene scene)
    {
      unsafe { return InternalCalls.Scene_SetActive(scene.Handle); }
    }

    /** The loaded scene at index (0 to SceneCount - 1), in load order. */
    public static Scene GetSceneAt(int index)
    {
      unsafe { return new Scene(InternalCalls.Scene_GetAt(index)); }
    }

    /** A loaded scene by name (case-insensitive); invalid when none is loaded. */
    public static Scene GetSceneByName(string name)
    {
      unsafe { return new Scene(InternalCalls.Scene_FindByName(name)); }
    }

    /** A loaded scene by path relative to the assets directory; the extension may be omitted. */
    public static Scene GetSceneByPath(string path)
    {
      unsafe { return new Scene(InternalCalls.Scene_FindByPath(path)); }
    }

    public static Scene GetSceneByBuildIndex(int buildIndex)
    {
      unsafe { return new Scene(InternalCalls.Scene_FindByBuildIndex(buildIndex)); }
    }

    /** Moves node's top-level node (with its children) into another loaded scene. */
    public static bool MoveNodeToScene(Node node, Scene scene)
    {
      unsafe { return InternalCalls.Scene_MoveNode(node.UUID, scene.Handle); }
    }

    /** Same as LoadScene(path). */
    public static void Load(string path)
    {
      LoadScene(path);
    }

    /** Saves the active scene's content to path (relative to the assets directory), (re-)registering it as a scene asset. */
    public static void Save(string path)
    {
      unsafe { InternalCalls.Scene_Save(path); }
    }

    /** Replaces every loaded scene with a fresh, empty one holding a camera, next frame. No unsaved-changes guard. */
    public static void New()
    {
      unsafe { InternalCalls.Scene_New(); }
    }

  } // class SceneManager

} // namespace Sbx.Core
