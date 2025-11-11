void draw()
{
  background(bgColor);

  // Draw some example content
  fill(fgColor);
  ellipse(mouseX, mouseY, 50, 50);

  displayInfo();
  println("" + frameCount);
  if (frameCount == 300)
  {
  	int a = 0 / 0;
  }
}
