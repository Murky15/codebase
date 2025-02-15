import random

workouts = ['push-ups', 'squats', 'v-ups', 'bicep curls', 'burpees', 'leg extensions', 'lat pulldown', 'bench press']

def generate_workout(time):
  time_remaining = time
  i = 0
  while time_remaining > 0:
    time_slot = random.randint(1, time_remaining)
    print(f'{i+1}. {workouts[i]}: {time_slot} minutes')
    time_remaining -= time_slot
    i = (i + 1) % len(workouts)

def main():
  time = int(input("How much time is your workout (minutes)? "))
  generate_workout(time)
  
main()