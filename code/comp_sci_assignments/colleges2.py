colleges = {'penn state':20000, 'colgate':3000, 'cornell':15000, 'binghamton':8000}

def find_colleges(e):
  for college in colleges:
    if colleges[college] > e:
      print(college)

def main():
  n = int(input('What is the minimum enrollment you want? '))
  find_colleges(n)
  
main()