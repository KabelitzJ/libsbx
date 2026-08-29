using System;
using System.IO;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

using Microsoft.CodeAnalysis;
using Microsoft.CodeAnalysis.CSharp;
using Microsoft.CodeAnalysis.Emit;

using Sbx.Managed.Interop;

namespace Sbx.Compiler
{

  /// <summary>
  /// In-process Roslyn compilation, invoked from C++ via the same hostfxr
  /// load_assembly_and_get_function_pointer bootstrap runtime::initialize_managed() already uses
  /// for Sbx.Managed.Host.Initialize — see runtime::compile_scripts. Kept as its own component
  /// (not folded into Sbx.Managed) so the Roslyn dependency only ever loads when scripts actually
  /// need compiling.
  /// </summary>
  public static class Compiler
  {

    [UnmanagedCallersOnly]
    private static unsafe Bool32 Compile(
      NativeString* sourcePaths, int sourceCount,
      NativeString* referencePaths, int referenceCount,
      NativeString outputPath,
      delegate*<Bool32, NativeString, int, int, NativeString, void> onDiagnostic)
    {
      try
      {
        var trees = new SyntaxTree[sourceCount];

        for (var i = 0; i < sourceCount; i++)
        {
          var path = sourcePaths[i].ToString();
          trees[i] = CSharpSyntaxTree.ParseText(File.ReadAllText(path), path: path);
        }

        // Caller-supplied references (Sbx.Core.dll) plus the installed shared framework's own
        // assemblies — resolves off whatever runtime is already hosting this process, no
        // dependency on a separately-installed Microsoft.NETCore.App.Ref pack.
        var frameworkDlls = Directory.GetFiles(System.Runtime.InteropServices.RuntimeEnvironment.GetRuntimeDirectory(), "*.dll");
        var references = new MetadataReference[referenceCount + frameworkDlls.Length];

        for (var i = 0; i < referenceCount; i++)
        {
          references[i] = MetadataReference.CreateFromFile(referencePaths[i].ToString());
        }

        for (var i = 0; i < frameworkDlls.Length; i++)
        {
          references[referenceCount + i] = MetadataReference.CreateFromFile(frameworkDlls[i]);
        }

        var options = new CSharpCompilationOptions(OutputKind.DynamicallyLinkedLibrary);
        var compilation = CSharpCompilation.Create("Game", trees, references, options);

        var outPath = outputPath.ToString();
        EmitResult result;

        using (var stream = File.Create(outPath))
        {
          result = compilation.Emit(stream);
        }

        foreach (var diagnostic in result.Diagnostics)
        {
          if (diagnostic.Severity < DiagnosticSeverity.Warning)
            continue;

          var span = diagnostic.Location.GetLineSpan();

          using NativeString file = span.Path ?? string.Empty;
          using NativeString message = diagnostic.GetMessage();

          onDiagnostic(diagnostic.Severity == DiagnosticSeverity.Error, file, span.StartLinePosition.Line + 1, span.StartLinePosition.Character + 1, message);
        }

        if (!result.Success && File.Exists(outPath))
        {
          // Never leave a broken/partial DLL in place of a previously-good one.
          File.Delete(outPath);
        }

        return result.Success;
      }
      catch (Exception exception)
      {
        using NativeString file = string.Empty;
        using NativeString message = exception.ToString();

        onDiagnostic(true, file, 0, 0, message);

        return false;
      }
    }

  }

} // namespace Sbx.Compiler
