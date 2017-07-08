class Median(object):

  def __init__(self, numValues):
    self.values = []
    self.numValues = numValues

  def add_variable(self, x):
    self.values.append(x)
    if (len(self.values) > self.numValues):
      self.values.pop(0)

  def get_median(self):
    lst = self.values
    quotient, remainder = divmod(len(lst), 2)
    if remainder:
        return sorted(lst)[quotient]
    return sum(sorted(lst)[quotient - 1:quotient + 1]) / 2.
