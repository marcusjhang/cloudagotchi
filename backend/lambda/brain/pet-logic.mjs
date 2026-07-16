// The rules of pet life — shared by both Lambdas. Pure functions, no AWS.

// Stats decay per hour of real time. Tuned so a full day of neglect
// makes the pet visibly sad (worst stat ~30) without starving it to zero.
const DECAY_PER_HOUR = { hunger: 2, energy: 1.5, mood: 1 };

// What each interaction does to the stats.
const INTERACTION_EFFECTS = {
  feed: { hunger: +30, energy: +5, mood: +5 },
  pet:  { hunger: 0, energy: 0, mood: +10 },
  play: { hunger: -10, energy: -15, mood: +20 }, // fun is exhausting
};

const clamp = (n) => Math.max(0, Math.min(100, Math.round(n)));

export function newbornPet(deviceId, now) {
  return { deviceId, hunger: 80, energy: 80, mood: 80, updatedAt: now };
}

/** Apply the passage of time: the cloud remembers, so the pet ages even powered off. */
export function applyDecay(pet, now) {
  const hours = Math.max(0, (now - pet.updatedAt) / 3_600_000);
  return {
    ...pet,
    hunger: clamp(pet.hunger - DECAY_PER_HOUR.hunger * hours),
    energy: clamp(pet.energy - DECAY_PER_HOUR.energy * hours),
    mood:   clamp(pet.mood   - DECAY_PER_HOUR.mood   * hours),
    updatedAt: now,
  };
}

/** Apply a human interaction on top of current (decayed) state. */
export function applyInteraction(pet, type) {
  const fx = INTERACTION_EFFECTS[type];
  if (!fx) return pet;
  return {
    ...pet,
    hunger: clamp(pet.hunger + fx.hunger),
    energy: clamp(pet.energy + fx.energy),
    mood:   clamp(pet.mood   + fx.mood),
  };
}

/** Reduce three stats to one face the device can render. */
export function moodOf(pet, hourOfDay) {
  if (hourOfDay >= 22 || hourOfDay < 7) return 'sleeping';
  const worst = Math.min(pet.hunger, pet.energy, pet.mood);
  if (worst > 60) return 'happy';
  if (worst > 30) return 'neutral';
  return 'sad';
}
