using System;
using System.IO;
using UnityEngine;

namespace Tickline.Forensics
{
    public static class InvestigationBundleLoader
    {
        public static BundleLoadResult LoadFromFile(string path)
        {
            if (string.IsNullOrWhiteSpace(path))
            {
                return Failure(
                    "bundle_path_empty",
                    "$",
                    "The investigation bundle path is empty.");
            }

            try
            {
                return LoadFromJson(File.ReadAllText(path));
            }
            catch (IOException exception)
            {
                return Failure(
                    "bundle_read_failed",
                    "$",
                    exception.Message);
            }
            catch (UnauthorizedAccessException exception)
            {
                return Failure(
                    "bundle_read_failed",
                    "$",
                    exception.Message);
            }
        }

        public static BundleLoadResult LoadFromJson(string json)
        {
            if (string.IsNullOrWhiteSpace(json))
            {
                return Failure(
                    "bundle_json_empty",
                    "$",
                    "The investigation bundle JSON is empty.");
            }

            InvestigationBundle bundle;

            try
            {
                bundle = JsonUtility.FromJson<InvestigationBundle>(json);
            }
            catch (ArgumentException exception)
            {
                return Failure(
                    "bundle_json_invalid",
                    "$",
                    exception.Message);
            }

            if (bundle == null)
            {
                return Failure(
                    "bundle_json_invalid",
                    "$",
                    "Unity could not deserialize the investigation bundle.");
            }

            return new BundleLoadResult(
                bundle,
                InvestigationBundleValidator.Validate(bundle));
        }

        private static BundleLoadResult Failure(
            string code,
            string path,
            string message)
        {
            var validation = new BundleValidationResult();
            validation.Add(code, path, message);
            return new BundleLoadResult(null, validation);
        }
    }
}
