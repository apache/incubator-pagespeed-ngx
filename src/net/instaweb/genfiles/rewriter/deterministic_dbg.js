(function(){var orig_date = Date, random_count = 0, date_count = 0, random_seed = 0.462, time_seed = 1204251968254;
Math.random = function() {
  random_count++;
  25 < random_count && (random_seed += 0.1, random_count = 1);
  return random_seed % 1;
};
Date = function() {
  if (this instanceof Date) {
    switch(date_count++, 25 < date_count && (time_seed += 50, date_count = 1), arguments.length) {
      case 0:
        return new orig_date(time_seed);
      case 1:
        return new orig_date(arguments[0]);
      default:
        return new orig_date(arguments[0], arguments[1], 3 <= arguments.length ? arguments[2] : 1, 4 <= arguments.length ? arguments[3] : 0, 5 <= arguments.length ? arguments[4] : 0, 6 <= arguments.length ? arguments[5] : 0, 7 <= arguments.length ? arguments[6] : 0);
    }
  }
  return(new Date).toString();
};
Date.__proto__ = orig_date;
orig_date.now = function() {
  return(new Date).getTime();
};
})();
