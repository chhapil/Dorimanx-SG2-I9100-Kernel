/*
 * Boostpulse support for ondemand based governors, based on Steve Kondik's solution for ondemand,
 * modified to work on multiple governors by Gustavo Santos, 2013.
 */

#define DEFAULT_FREQ_BOOST_TIME      (500000)
#define MAX_FREQ_BOOST_TIME         (5000000)

/* have the timer rate booted for this much time 3s*/
#define TIMER_RATE_BOOST_TIME 3000000

static u64 freq_boosted_time;
static int sampling_rate_boosted;
static u64 sampling_rate_boosted_time;
static unsigned int current_sampling_rate;
