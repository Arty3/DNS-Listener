# **************************************************************************** #

TARGET	= dns_listener

CC		= cc

CFLAGS	= -Wall -Wextra -Werror -Wstrict-prototypes		\
		  -Wmissing-prototypes -Wpedantic -std=gnu17	\
		  -fstack-protector-strong -O3

SRCS	= src/main.c
OBJS	= ${SRCS:.c=.o}

all: $(TARGET)

$(TARGET) : $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $(TARGET)

clean:
	@rm -f $(OBJS)

fclean: clean
	@rm -f $(TARGET)

re: fclean all

.PHONY: all clean fclean re
