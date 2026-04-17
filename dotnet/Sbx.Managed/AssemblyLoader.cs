using System;
using System.Collections.Generic;
using System.IO;
using System.IO.MemoryMappedFiles;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Runtime.Loader;

using Sbx.Managed.Interop;

namespace Sbx.Managed
{

  using static Host;

  public enum AssemblyLoadStatus
  {
    Success,
    FileNotFound,
    FileLoadFailure,
    InvalidFilePath,
    InvalidAssembly,
    UnknownError
  }

  public static class AssemblyLoader
  {
    private static readonly Dictionary<Type, AssemblyLoadStatus> _assemblyLoadErrorLookup = new();
    private static readonly Dictionary<int, AssemblyLoadContext?> _assemblyContexts = new();
    private static readonly Dictionary<int, Assembly> _assemblyCache = new();
    private static readonly Dictionary<int, List<GCHandle>> _allocatedHandles = new();
    private static AssemblyLoadStatus _lastLoadStatus = AssemblyLoadStatus.Success;

    private static readonly AssemblyLoadContext? _sbxAssemblyLoadContext;

    static AssemblyLoader()
    {
      _assemblyLoadErrorLookup.Add(typeof(BadImageFormatException), AssemblyLoadStatus.InvalidAssembly);
      _assemblyLoadErrorLookup.Add(typeof(FileNotFoundException), AssemblyLoadStatus.FileNotFound);
      _assemblyLoadErrorLookup.Add(typeof(FileLoadException), AssemblyLoadStatus.FileLoadFailure);
      _assemblyLoadErrorLookup.Add(typeof(ArgumentNullException), AssemblyLoadStatus.InvalidFilePath);
      _assemblyLoadErrorLookup.Add(typeof(ArgumentException), AssemblyLoadStatus.InvalidFilePath);

      _sbxAssemblyLoadContext = AssemblyLoadContext.GetLoadContext(typeof(AssemblyLoader).Assembly);
      _sbxAssemblyLoadContext!.Resolving += ResolveAssembly;

      CacheSbxAssemblies();
    }

    private static void CacheSbxAssemblies()
    {
      foreach (var assembly in _sbxAssemblyLoadContext!.Assemblies)
      {
        int assemblyId = assembly.GetName().Name!.GetHashCode();
        _assemblyCache.Add(assemblyId, assembly);
      }
    }

    internal static bool TryGetAssembly(int InAssemblyId, out Assembly? OutAssembly)
    {
      return _assemblyCache.TryGetValue(InAssemblyId, out OutAssembly);
    }

    internal static Assembly? ResolveAssembly(AssemblyLoadContext? InAssemblyLoadContext, AssemblyName InAssemblyName)
    {
      try
      {
        int assemblyId = InAssemblyName.Name!.GetHashCode();

        if (_assemblyCache.TryGetValue(assemblyId, out var cachedAssembly))
        {
          return cachedAssembly;
        }

        foreach (var loadContext in AssemblyLoadContext.All)
        {
          foreach (var assembly in loadContext.Assemblies)
          {
            if (assembly.GetName().Name != InAssemblyName.Name)
              continue;

            _assemblyCache.Add(assemblyId, assembly);
            return assembly;
          }
        }
      }
      catch (Exception ex)
      {
        Host.HandleException(ex);
      }

      return null;
    }

    [UnmanagedCallersOnly]
    internal static int CreateAssemblyLoadContext(NativeString InName)
    {
      string? name = InName;

      if (name == null)
        return -1;

      var alc = new AssemblyLoadContext(name, true);
      alc.Resolving += ResolveAssembly;
      alc.Unloading += ctx =>
      {
        foreach (var assembly in ctx.Assemblies)
        {
          var assemblyName = assembly.GetName();
          int assemblyId = assemblyName.Name!.GetHashCode();
          _assemblyCache.Remove(assemblyId);
        }
      };

      int contextId = name.GetHashCode();
      _assemblyContexts.Add(contextId, alc);
      return contextId;
    }

    [UnmanagedCallersOnly]
    internal static void UnloadAssemblyLoadContext(int InContextId)
    {
      if (!_assemblyContexts.TryGetValue(InContextId, out var alc))
      {
        LogMessage($"Cannot unload AssemblyLoadContext '{InContextId}', it was either never loaded or already unloaded.", MessageLevel.Warning);
        return;
      }

      if (alc == null)
      {
        LogMessage($"AssemblyLoadContext '{InContextId}' was found in dictionary but was null. This is most likely a bug.", MessageLevel.Error);
        return;
      }

      foreach (var assembly in alc.Assemblies)
      {
        var assemblyName = assembly.GetName();
        int assemblyId = assemblyName.Name!.GetHashCode();

        if (!_allocatedHandles.TryGetValue(assemblyId, out var handles))
        {
          continue;
        }

        foreach (var handle in handles)
        {
          if (!handle.IsAllocated || handle.Target == null)
          {
            continue;
          }

          LogMessage($"Found unfreed object '{handle.Target}' from assembly '{assemblyName}'. Deallocating.", MessageLevel.Warning);
          handle.Free();
        }

        _allocatedHandles.Remove(assemblyId);
      }

      Object._cachedMethods.Clear();

      TypeInterface._cachedTypes.Clear();
      TypeInterface._cachedMethods.Clear();
      TypeInterface._cachedFields.Clear();
      TypeInterface._cachedProperties.Clear();
      TypeInterface._cachedAttributes.Clear();

      _assemblyContexts.Remove(InContextId);
      alc.Unload();
    }

    [UnmanagedCallersOnly]
    internal static int LoadAssembly(int InContextId, NativeString InAssemblyFilePath)
    {
      try
      {
        if (string.IsNullOrEmpty(InAssemblyFilePath))
        {
          _lastLoadStatus = AssemblyLoadStatus.InvalidFilePath;
          return -1;
        }

        if (!File.Exists(InAssemblyFilePath))
        {
          LogMessage($"Failed to load assembly '{InAssemblyFilePath}', file not found.", MessageLevel.Error);
          _lastLoadStatus = AssemblyLoadStatus.FileNotFound;
          return -1;
        }

        if (!_assemblyContexts.TryGetValue(InContextId, out var alc))
        {
          LogMessage($"Failed to load assembly '{InAssemblyFilePath}', couldn't find AssemblyLoadContext with id {InContextId}.", MessageLevel.Error);
          _lastLoadStatus = AssemblyLoadStatus.UnknownError;
          return -1;
        }

        if (alc == null)
        {
          LogMessage($"Failed to load assembly '{InAssemblyFilePath}', AssemblyLoadContext with id {InContextId} was null.", MessageLevel.Error);
          _lastLoadStatus = AssemblyLoadStatus.UnknownError;
          return -1;
        }

        Assembly? assembly = null;

        using (var file = MemoryMappedFile.CreateFromFile(InAssemblyFilePath!))
        {
          using var stream = file.CreateViewStream();
          assembly = alc.LoadFromStream(stream);
        }

        LogMessage($"Loading assembly '{InAssemblyFilePath}'", MessageLevel.Info);
        var assemblyName = assembly.GetName();
        int assemblyId = assemblyName.Name!.GetHashCode();
        _assemblyCache.Add(assemblyId, assembly);
        _lastLoadStatus = AssemblyLoadStatus.Success;
        return assemblyId;
      }
      catch (Exception ex)
      {
        _assemblyLoadErrorLookup.TryGetValue(ex.GetType(), out _lastLoadStatus);
        HandleException(ex);
        return -1;
      }
    }

    [UnmanagedCallersOnly]
    internal static unsafe int LoadAssemblyFromMemory(int InContextId, byte* data, long dataLength)
    {
      try
      {
        if (!_assemblyContexts.TryGetValue(InContextId, out var alc))
        {
          LogMessage($"Failed to load assembly, couldn't find AssemblyLoadContext with id {InContextId}.", MessageLevel.Error);
          _lastLoadStatus = AssemblyLoadStatus.UnknownError;
          return -1;
        }

        if (alc == null)
        {
          LogMessage($"Failed to load assembly, couldn't find AssemblyLoadContext with id {InContextId} was null.", MessageLevel.Error);
          _lastLoadStatus = AssemblyLoadStatus.UnknownError;
          return -1;
        }

        Assembly? assembly = null;

        using (var stream = new UnmanagedMemoryStream(data, dataLength))
        {
          assembly = alc.LoadFromStream(stream);
        }

        LogMessage($"Loading assembly '{assembly.FullName}'", MessageLevel.Info);
        var assemblyName = assembly.GetName();
        int assemblyId = assemblyName.Name!.GetHashCode();
        _assemblyCache.Add(assemblyId, assembly);
        _lastLoadStatus = AssemblyLoadStatus.Success;
        return assemblyId;
      }
      catch (Exception ex)
      {
        _assemblyLoadErrorLookup.TryGetValue(ex.GetType(), out _lastLoadStatus);
        HandleException(ex);
        return -1;
      }
    }

    [UnmanagedCallersOnly]
    internal static AssemblyLoadStatus GetLastLoadStatus() => _lastLoadStatus;

    [UnmanagedCallersOnly]
    internal static NativeString GetAssemblyName(int InAssemblyId)
    {
      if (!_assemblyCache.TryGetValue(InAssemblyId, out var assembly))
      {
        LogMessage($"Couldn't get assembly name for assembly '{InAssemblyId}', assembly not in dictionary.", MessageLevel.Error);
        return "";
      }

      var assemblyName = assembly.GetName();
      return assemblyName.Name;
    }

    internal static void RegisterHandle(Assembly InAssembly, GCHandle InHandle)
    {
      var assemblyName = InAssembly.GetName();
      int assemblyId = assemblyName.Name!.GetHashCode();

      if (!_allocatedHandles.TryGetValue(assemblyId, out var handles))
      {
        _allocatedHandles.Add(assemblyId, new List<GCHandle>());
        handles = _allocatedHandles[assemblyId];
      }

      handles.Add(InHandle);
    }

  }

} // namespace Sbx.Managed