using System.IO;
using System.Linq;
using NUnit.Framework;
using UnityEngine;

namespace Tickline.Forensics.Tests
{
    public sealed class InvestigationBundleLoaderTests
    {
        [Test]
        public void ValidFixtureLoadsAndValidates()
        {
            var result = InvestigationBundleLoader.LoadFromFile(FixturePath());

            Assert.That(
                result.Validation.IsValid,
                Is.True,
                FormatIssues(result.Validation));

            Assert.That(result.Bundle, Is.Not.Null);
            Assert.That(result.Bundle.schemaVersion, Is.EqualTo(1));
            Assert.That(result.Bundle.sessions.Length, Is.EqualTo(1));
            Assert.That(result.Bundle.evidence.Length, Is.EqualTo(4));
            Assert.That(result.Bundle.replay.verified, Is.True);
        }

        [Test]
        public void BrokenEvidenceLinkIsRejected()
        {
            var result = InvestigationBundleLoader.LoadFromFile(FixturePath());

            result.Bundle.evidence[1].previousDigest =
                "dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd";

            var validation =
                InvestigationBundleValidator.Validate(result.Bundle);

            Assert.That(validation.IsValid, Is.False);
            Assert.That(
                validation.Issues.Any(
                    issue => issue.Code == "evidence_chain_broken"),
                Is.True);
        }

        [Test]
        public void InvalidUnsignedValueIsRejected()
        {
            var result = InvestigationBundleLoader.LoadFromFile(FixturePath());

            result.Bundle.evidence[0].targetTick = "-1";

            var validation =
                InvestigationBundleValidator.Validate(result.Bundle);

            Assert.That(validation.IsValid, Is.False);
            Assert.That(
                validation.Issues.Any(
                    issue => issue.Code == "uint64_invalid"),
                Is.True);
        }

        [Test]
        public void UnsupportedSchemaIsRejected()
        {
            var result = InvestigationBundleLoader.LoadFromFile(FixturePath());

            result.Bundle.schemaVersion = 99;

            var validation =
                InvestigationBundleValidator.Validate(result.Bundle);

            Assert.That(validation.IsValid, Is.False);
            Assert.That(
                validation.Issues.Any(
                    issue => issue.Code == "schema_unsupported"),
                Is.True);
        }

        [Test]
        public void EmptyJsonProducesStableFailure()
        {
            var result = InvestigationBundleLoader.LoadFromJson(string.Empty);

            Assert.That(result.Bundle, Is.Null);
            Assert.That(result.Validation.IsValid, Is.False);
            Assert.That(
                result.Validation.Issues.Single().Code,
                Is.EqualTo("bundle_json_empty"));
        }

        private static string FixturePath()
        {
            return Path.Combine(
                Application.dataPath,
                "Tickline",
                "Forensics",
                "Tests",
                "EditMode",
                "Fixtures",
                "valid-investigation-bundle.json");
        }

        private static string FormatIssues(BundleValidationResult validation)
        {
            return string.Join(
                "\n",
                validation.Issues.Select(
                    issue =>
                        $"{issue.Code} at {issue.Path}: {issue.Message}"));
        }
    }
}
