colleges = ['penn state', 'colgate', 'cornell', 'binghamton']
enrollment = [20000, 3000, 15000, 8000]

def find_colleges(e):
  for i in range(len(enrollment)):
    if enrollment[i] > e:
      print(colleges[i])

def main():
  n = int(input('What is the minimum enrollment you want? '))
  find_colleges(n)
  
main()