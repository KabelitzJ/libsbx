using System;
using System.Collections;
using System.Collections.Generic;
using System.Diagnostics.CodeAnalysis;
using System.Reflection;
using System.Runtime.InteropServices;

namespace Sbx.Managed.Interop
{

  public sealed class NativeArrayEnumerator<T> : IEnumerator<T>
  {
    private readonly T[] _elements;
    private int _index = -1;

    public NativeArrayEnumerator(T[] elements)
    {
      _elements = elements;
    }

    public bool MoveNext()
    {
      _index++;
      return _index < _elements.Length;
    }

    void IEnumerator.Reset() => _index = -1;
    void IDisposable.Dispose()
    {
      _index = -1;
      GC.SuppressFinalize(this);
    }

    object IEnumerator.Current => Current!;

    public T Current => _elements[_index];

  }

  [StructLayout(LayoutKind.Sequential, Pack = 1)]
  public struct NativeArray<T> : IDisposable, IEnumerable<T>
  {
    private IntPtr _nativeArray;
    private IntPtr _arrayHandle;
    private int _nativeLength;

    private Bool32 _isDisposed;

    public int Length => _nativeLength;

    public NativeArray(int InLength)
    {
      _nativeArray = Marshal.AllocHGlobal(InLength * Marshal.SizeOf<T>());
      _nativeLength = InLength;
    }

    public NativeArray([DisallowNull] T?[] InValues)
    {
      _nativeArray = Marshal.AllocHGlobal(InValues.Length * Marshal.SizeOf<T>());
      _nativeLength = InValues.Length;

      for (int i = 0; i < _nativeLength; i++)
      {
        var elem = InValues[i];

        if (elem == null)
          continue;

        Marshal.StructureToPtr(elem, IntPtr.Add(_nativeArray, i * Marshal.SizeOf<T>()), false);
      }
    }

    internal NativeArray(IntPtr InArray, IntPtr InHandle, int InLength)
    {
      _nativeArray = InArray;
      _arrayHandle = InHandle;
      _nativeLength = InLength;
    }

    public T[] ToArray()
    {
      Span<T> data = Span<T>.Empty;

      if (_nativeArray != IntPtr.Zero && _nativeLength > 0)
      {
        unsafe { data = new Span<T>(_nativeArray.ToPointer(), _nativeLength); }
      }

      return data.ToArray();
    }

    public Span<T> ToSpan()
    {
      unsafe { return new Span<T>(_nativeArray.ToPointer(), _nativeLength); }
    }

    public ReadOnlySpan<T> ToReadOnlySpan() => ToSpan();

    public void Dispose()
    {
      if (!_isDisposed && _arrayHandle == IntPtr.Zero)
      {
        Marshal.FreeHGlobal(_nativeArray);
        _isDisposed = true;
      }

      GC.SuppressFinalize(this);
    }

    public IEnumerator<T> GetEnumerator() => new NativeArrayEnumerator<T>(this);
    IEnumerator IEnumerable.GetEnumerator() => new NativeArrayEnumerator<T>(this);

    public T? this[int InIndex]
    {
      get => Marshal.PtrToStructure<T>(IntPtr.Add(_nativeArray, InIndex * Marshal.SizeOf<T>()));
      set => Marshal.StructureToPtr<T>(value!, IntPtr.Add(_nativeArray, InIndex * Marshal.SizeOf<T>()), false);
    }

    public static NativeArray<T> Map(T[] array)
    {
      var handle = GCHandle.Alloc(array, GCHandleType.Pinned);
      return new(handle.AddrOfPinnedObject(), GCHandle.ToIntPtr(handle), array.Length);
    }

    public static void Unmap(ref NativeArray<T> array)
    {
      GCHandle.FromIntPtr(array._arrayHandle).Free();
      array._nativeArray = IntPtr.Zero;
      array._arrayHandle = IntPtr.Zero;
      array._nativeLength = 0;
    }

    public static implicit operator T[](NativeArray<T> InArray) => InArray.ToArray();
    public static implicit operator NativeArray<T>(T[] InArray) => new(InArray);

  }

  public static class ArrayStorage
  {
    private static Dictionary<int, GCHandle> _fieldArrays = new();

    public static bool HasFieldArray(object? InTarget, MemberInfo? InArrayMemberInfo)
    {
      if (InArrayMemberInfo == null)
        return false;

      int arrayId = InArrayMemberInfo.GetHashCode();
      arrayId += InTarget != null ? InTarget.GetHashCode() : 0;
      return _fieldArrays.ContainsKey(arrayId);
    }

    public static GCHandle? GetFieldArray(object? InTarget, object? InValue, MemberInfo? InArrayMemberInfo)
    {
      if (InArrayMemberInfo == null)
        return null;

      int arrayId = InArrayMemberInfo.GetHashCode();
      arrayId += InTarget != null ? InTarget.GetHashCode() : 0;

      if (!_fieldArrays.TryGetValue(arrayId, out var arrayHandle))
      {
        var arrayObject = InValue as Array;
        arrayHandle = GCHandle.Alloc(arrayObject, GCHandleType.Pinned);
        _fieldArrays.Add(arrayId, arrayHandle);
      }

      return arrayHandle;
    }
  }

  [StructLayout(LayoutKind.Sequential)]
  public struct NativeInstance<T>
  {
    private readonly IntPtr _handle;
    private readonly IntPtr _unused;

    private NativeInstance(IntPtr handle)
    {
      _handle = handle;
      _unused = IntPtr.Zero;
    }

    public T? Get()
    {
      if (_handle == IntPtr.Zero)
        return default;

      GCHandle handle = GCHandle.FromIntPtr(_handle);

      if (!(handle.Target is T))
        return default;

      return (T)handle.Target;
    }

    public static implicit operator NativeInstance<T>(T instance)
    {
      return new(GCHandle.ToIntPtr(GCHandle.Alloc(instance, GCHandleType.Pinned)));
    }

    public static implicit operator T?(NativeInstance<T> InInstance)
    {
      return InInstance.Get();
    }
  }

  [StructLayout(LayoutKind.Sequential)]
  public struct NativeString : IDisposable
  {
    internal IntPtr _nativeString;
    private Bool32 _isDisposed;

    public void Dispose()
    {
      if (!_isDisposed)
      {
        if (_nativeString != IntPtr.Zero)
        {
          Marshal.FreeCoTaskMem(_nativeString);
          _nativeString = IntPtr.Zero;
        }

        _isDisposed = true;
      }

      GC.SuppressFinalize(this);
    }

    public override string? ToString() => this;

    public static NativeString Null() => new NativeString() { _nativeString = IntPtr.Zero };

    public static implicit operator NativeString(string? InString) => new() { _nativeString = Marshal.StringToCoTaskMemAuto(InString) };
    public static implicit operator string?(NativeString InString) => Marshal.PtrToStringAuto(InString._nativeString);
  }

  [StructLayout(LayoutKind.Sequential, Pack = 1)]
  public struct Bool32
  {
    public uint Value { get; set; }

    public static implicit operator Bool32(bool InValue) => new() { Value = InValue ? 1u : 0u };
    public static implicit operator bool(Bool32 InBool32) => InBool32.Value > 0;
  }

  [StructLayout(LayoutKind.Sequential, Pack = 1)]
  public struct ReflectionType
  {
    private readonly int m_TypeId;

    public int ID => m_TypeId;

    public ReflectionType(int InTypeID)
    {
      m_TypeId = InTypeID;
    }

    public static implicit operator ReflectionType(Type? InType) => new(TypeInterface._cachedTypes.Add(InType));

  }

} // namespace Sbx.Managed.Interop