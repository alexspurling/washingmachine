class Variance(object):

  def __init__(self, numValues):
    self.K = 0
    self.n = 0
    self.Ex = 0
    self.Ex2 = 0
    self.values = []
    self.numValues = numValues

  def add_variable(self, x):
    if (self.n == 0):
      self.K = x
    self.n = self.n + 1
    self.Ex += x - self.K
    self.Ex2 += (x - self.K) * (x - self.K)
    self.values.append(x)
    if (self.n > self.numValues):
      self.remove_variable(self.values.pop(0))

  def remove_variable(self, x):
    self.n = self.n - 1
    self.Ex -= (x - self.K)
    self.Ex2 -= (x - self.K) * (x - self.K)

  def get_meanvalue(self):
    return self.K + self.Ex / self.n

  def get_max(self):
    return max(self.values) - min(self.values)

  def get_variance(self):
    if self.n > 1:
      return (self.Ex2 - (self.Ex*self.Ex)/self.n) / (self.n-1)
    return 0
