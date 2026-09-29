using System;
using System.Collections.Generic;

namespace Sbx.Managed
{

  /// <summary>
  /// Hands native code a stable int id per object. Ids come from a counter, not the object's hash code (identity hashes aren't
  /// unique, so two live objects could collide and native code would read the wrong one); adding an object that's already here
  /// returns its existing id. The counter survives Clear(), so an id handed out before a script reload never resolves to a
  /// different object afterwards.
  /// </summary>
  public class UniqueIdList<T> where T : class
  {
    private readonly Dictionary<int, T> _objects = new();
    private readonly Dictionary<T, int> _ids = new(ReferenceEqualityComparer.Instance);
    private readonly object _lock = new();
    private int _nextId = 1;

    public bool Contains(int id)
    {
      lock (_lock)
      {
        return _objects.ContainsKey(id);
      }
    }

    public int Add(T? obj)
    {
      if (obj == null)
      {
        throw new ArgumentNullException(nameof(obj));
      }

      lock (_lock)
      {
        if (_ids.TryGetValue(obj, out var existing))
        {
          return existing;
        }

        var id = _nextId++;

        _objects.Add(id, obj);
        _ids.Add(obj, id);

        return id;
      }
    }

    public bool TryGetValue(int id, out T? obj)
    {
      lock (_lock)
      {
        return _objects.TryGetValue(id, out obj);
      }
    }

    public void Clear()
    {
      lock (_lock)
      {
        _objects.Clear();
        _ids.Clear();
      }
    }
  }

} // namespace Sbx.Managed
