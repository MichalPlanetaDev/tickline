using System;

namespace Tickline.Forensics
{
    public sealed class ReplayPlaybackController
    {
        private readonly ReplayTimeline timeline;
        private readonly double secondsPerEntry;

        private double accumulatedSeconds;
        private int currentIndex;

        public ReplayPlaybackController(
            ReplayTimeline timeline,
            double secondsPerEntry = 0.5)
        {
            this.timeline = timeline ??
                throw new ArgumentNullException(nameof(timeline));

            if (double.IsNaN(secondsPerEntry) ||
                double.IsInfinity(secondsPerEntry) ||
                secondsPerEntry <= 0.0)
            {
                throw new ArgumentOutOfRangeException(
                    nameof(secondsPerEntry));
            }

            this.secondsPerEntry = secondsPerEntry;
            currentIndex = timeline.Count == 0 ? -1 : 0;
            PlaybackSpeed = 1.0;
        }

        public event Action<int> PositionChanged;

        public ReplayTimeline Timeline => timeline;

        public int CurrentIndex => currentIndex;

        public ReplayTimelineEntry CurrentEntry =>
            currentIndex >= 0
                ? timeline[currentIndex]
                : null;

        public bool IsPlaying { get; private set; }

        public double PlaybackSpeed { get; private set; }

        public double SecondsPerEntry => secondsPerEntry;

        public bool Play()
        {
            if (timeline.Count == 0 ||
                currentIndex >= timeline.Count - 1)
            {
                IsPlaying = false;
                return false;
            }

            IsPlaying = true;
            return true;
        }

        public void Pause()
        {
            IsPlaying = false;
            accumulatedSeconds = 0.0;
        }

        public void SetPlaybackSpeed(double speed)
        {
            if (double.IsNaN(speed) ||
                double.IsInfinity(speed) ||
                speed <= 0.0)
            {
                throw new ArgumentOutOfRangeException(nameof(speed));
            }

            PlaybackSpeed = speed;
        }

        public int Advance(double elapsedSeconds)
        {
            if (double.IsNaN(elapsedSeconds) ||
                double.IsInfinity(elapsedSeconds) ||
                elapsedSeconds < 0.0)
            {
                throw new ArgumentOutOfRangeException(
                    nameof(elapsedSeconds));
            }

            if (!IsPlaying || timeline.Count == 0)
            {
                return 0;
            }

            accumulatedSeconds +=
                elapsedSeconds * PlaybackSpeed;

            var advancedEntries = 0;

            while (IsPlaying &&
                   accumulatedSeconds >= secondsPerEntry)
            {
                accumulatedSeconds -= secondsPerEntry;

                if (!MoveTo(currentIndex + 1))
                {
                    Pause();
                    break;
                }

                advancedEntries++;

                if (currentIndex >= timeline.Count - 1)
                {
                    Pause();
                }
            }

            return advancedEntries;
        }

        public bool StepForward()
        {
            Pause();
            return MoveTo(currentIndex + 1);
        }

        public bool StepBackward()
        {
            Pause();
            return MoveTo(currentIndex - 1);
        }

        public bool Seek(int index)
        {
            Pause();

            if (index < 0 || index >= timeline.Count)
            {
                return false;
            }

            return MoveTo(index);
        }

        public bool SeekToTick(ulong targetTick)
        {
            var index =
                timeline.FindNearestIndexToTick(targetTick);

            return index >= 0 && Seek(index);
        }

        public void Reset()
        {
            Pause();

            var initialIndex =
                timeline.Count == 0 ? -1 : 0;

            if (currentIndex == initialIndex)
            {
                return;
            }

            currentIndex = initialIndex;
            PositionChanged?.Invoke(currentIndex);
        }

        private bool MoveTo(int index)
        {
            if (index < 0 || index >= timeline.Count)
            {
                return false;
            }

            if (currentIndex == index)
            {
                return true;
            }

            currentIndex = index;
            PositionChanged?.Invoke(currentIndex);
            return true;
        }
    }
}
