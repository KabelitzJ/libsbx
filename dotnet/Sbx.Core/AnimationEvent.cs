namespace Sbx.Core
{

  /** An event a clip played past, as authored on the clip in the mesh's import settings. */
  public readonly struct AnimationEvent
  {

    public string Name { get; }
    public float FloatParameter { get; }
    public int IntParameter { get; }
    public string StringParameter { get; }

    /** The clip it's on. */
    public string ClipName { get; }

    /** The animator layer that played it (0 is the base layer). */
    public int Layer { get; }

    /** The clip's weight when it fired: its share of blend tree, crossfade and layer. */
    public float Weight { get; }

    public AnimationEvent(string name, float floatParameter, int intParameter, string stringParameter, string clipName, int layer, float weight)
    {
      Name = name;
      FloatParameter = floatParameter;
      IntParameter = intParameter;
      StringParameter = stringParameter;
      ClipName = clipName;
      Layer = layer;
      Weight = weight;
    }

  } // struct AnimationEvent

} // namespace Sbx.Core
